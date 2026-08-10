#include "tage2.h"

#include <random>

// Computes the index and tag used to access entries in a specific tage2 table
std::pair<std::size_t, uint8_t> tage2::compute_index_and_tag(champsim::address ip, std::size_t table_idx) const
{
  constexpr std::size_t INDEX_BITS = 9; // How many bits the table index can be (2^9 = 512)

  std::size_t ip_val = ip.to<std::size_t>(); // Convert IP to integer
  std::size_t folded_history = 0;

  // Fold global history to reduce size and include historical influence
  for (std::size_t i = 0; i < HISTORY_LENGTHS[table_idx]; ++i) {
    folded_history ^= (global_history[i] << (i % INDEX_BITS));
  }

  // Calculate index into the table (hash of IP and folded history)
  std::size_t index = (ip_val ^ folded_history) % TABLE_SIZE;

  // Tag is a compact identifier used to validate a match
  uint8_t tag = static_cast<uint8_t>((ip_val >> 2) ^ folded_history) & ((1 << TAG_BITS) - 1);
  return {index, tag};
}

bool tage2::predict_branch(champsim::address ip, champsim::address predicted_target, bool always_taken, uint8_t branch_type)
{
  // Static prediction: always taken
  if (always_taken)
    return true;

  // Predict direct jumps and calls as taken
  if (branch_type == BRANCH_DIRECT_JUMP || branch_type == BRANCH_DIRECT_CALL)
    return true;

  // All other branches, including returns, use TAGE prediction
  return predict_branch(ip);
}

// Predict whether a branch will be taken or not
bool tage2::predict_branch(champsim::address ip)
{
  std::optional<std::pair<std::size_t, TageEntry>> primary_match;
  std::optional<std::pair<std::size_t, TageEntry>> alt_match;

  std::optional<bool> pred = std::nullopt;

  // Search for a matching entry (start from longest history to shortest)
  for (int i = NUM_TAGGED_TABLES - 1; i >= 0; --i) {
    auto [index, tag] = compute_index_and_tag(ip, i);
    const auto& entry = tagged_tables[i][index];

    if (entry.valid && entry.tag == tag) {
      return entry.counter.value() >= (entry.counter.maximum / 2); // Use prediction directly
    }
  }

  // Identify primary and alternate matching entries (to apply confidence rules)
  for (int i = NUM_TAGGED_TABLES - 1; i >= 0; --i) {
    auto [index, tag] = compute_index_and_tag(ip, i);
    const auto& entry = tagged_tables[i][index];
    if (entry.valid && entry.tag == tag) {
      if (!primary_match.has_value()) {
        primary_match = {i, entry}; // First match (longest)
      } else if (!alt_match.has_value()) {
        alt_match = {i, entry}; // Next match (shorter)
      }
    }
  }

  if (primary_match.has_value()) {
    const auto& [idx, entry] = primary_match.value();
    bool weak = entry.counter.value() == entry.counter.maximum / 2;

    if (weak && alt_match.has_value()) {
      return alt_match->second.counter.value() >= (alt_match->second.counter.maximum / 2);
    } else {
      return entry.counter.value() >= (entry.counter.maximum / 2);
    }
  }

  // Fallback to bimodal if no matching entry
  if (!pred.has_value()) {
    auto idx = bimodal_index(ip);
    pred = bimodal_table[idx].value() >= (bimodal_table[idx].maximum / 2);
  }

  return pred.value();
}

// Update predictor with the actual outcome of the last branch
void tage2::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
  // Step 1: Search for matching entry
  int hit_index = -1;
  std::size_t matched_table_idx = 0;
  std::size_t matched_tag = 0;

  for (int i = NUM_TAGGED_TABLES - 1; i >= 0; --i) {
    auto [index, tag] = compute_index_and_tag(ip, i);
    auto& entry = tagged_tables[i][index];

    if (entry.valid && entry.tag == tag) {
      hit_index = index;
      matched_table_idx = i;
      matched_tag = tag;
      break;
    }
  }

  // Step 2: If found, train the matching entry by updating its counter
  if (hit_index != -1) {
    auto& entry = tagged_tables[matched_table_idx][hit_index];

    if (taken)
      entry.counter += 1; // Strengthen taken
    else
      entry.counter -= 1; // Strengthen not-taken

    entry.useful = true; // Mark this entry as useful
  } else {
    // Step 3: No match — try inserting new entry into a random tage2 table
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, NUM_TAGGED_TABLES - 1);

    for (int attempts = 0; attempts < NUM_TAGGED_TABLES; ++attempts) {
      std::size_t i = dist(gen); // random table index
      auto [index, tag] = compute_index_and_tag(ip, i);
      auto& entry = tagged_tables[i][index];

      if (!entry.valid || !entry.useful) {
        entry.valid = true;
        entry.tag = tag;
        entry.counter = taken ? 1 : 0; // Initialize based on outcome
        entry.useful = false;          // Mark as untested
        break;
      }
    }
  }

  // Step 4: Update global history
  global_history <<= 1;         // Shift left (drop oldest bit)
  global_history.set(0, taken); // Insert newest outcome

  // Update the fallback bimodal predictor as well
  auto idx = bimodal_index(ip);
  bimodal_table[idx] += taken ? 1 : -1;

  // Occasionally decay usefulness bits to prevent stale entries from staying forever
  if (++update_counter % 1024 == 0)
    age_usefulness_bits();
}
