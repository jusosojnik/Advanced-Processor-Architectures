#include "mybranch.h"

#include <random>

// Computes the index and tag used to access entries in a specific mybranch table
std::pair<std::size_t, uint8_t> mybranch::compute_index_and_tag(champsim::address ip, std::size_t table_idx) const
{
  constexpr std::size_t INDEX_BITS = 10; // How many bits the table index can be (2^10 = 1024)

  std::size_t ip_val = ip.to<std::size_t>(); // Convert IP to integer
  std::size_t folded_history = 0;

  // Fold global history to reduce size and include historical influence
  for (std::size_t i = 0; i < HISTORY_LENGTHS[table_idx]; ++i) {
    folded_history ^= (global_history[i] << (i % INDEX_BITS));
  }

  // Calculate index into the table (hash of IP and folded history)
  std::size_t index = (ip_val ^ folded_history) % TABLE_SIZE;

  // Tag is a compact identifier used to validate a match
  uint16_t tag = static_cast<uint16_t>((ip_val >> 2) ^ folded_history) & ((1 << TAG_BITS) - 1);
  return {index, tag};
}

// bool mybranch::predict_branch(champsim::address ip, champsim::address predicted_target, bool always_taken, uint8_t branch_type)
// {
//   // Short-circuit based on branch type or static always-taken info
//   if (always_taken)
//     return true;

//   if (branch_type == BRANCH_DIRECT_JUMP || branch_type == BRANCH_DIRECT_CALL)
//     return true;

//   // Use RAS for returns
//   if (branch_type == BRANCH_RETURN && !ras.empty()) {
//     return predicted_target == ras.top();
//   }

//   // Loop Count Prediction
//   std::size_t ip_val = ip.to<std::size_t>();
//   auto& loop = loop_predictor[ip_val];

//   if (!is_recent_loop_exit(ip_val) && loop.initialized && loop.current_iter < loop.iteration_count)
//     return true;

//   // Use full mybranch logic otherwise
//   return predict_branch(ip);
// }

bool mybranch::predict_branch(champsim::address ip, champsim::address predicted_target, bool always_taken, uint8_t branch_type)
{
  // Static prediction: always taken
  if (always_taken)
    return true;

  // Predict direct jumps and calls as taken
  if (branch_type == BRANCH_DIRECT_JUMP || branch_type == BRANCH_DIRECT_CALL)
    return true;

  // Use RAS for returns
  if (branch_type == BRANCH_RETURN && !ras.empty()) {
    return predicted_target == ras.top();
  }

  // Simple loop bias tracking
  std::size_t ip_val = ip.to<std::size_t>();
  auto it = loop_bias.find(ip_val);
  if (it != loop_bias.end() && it->second.total >= 10) {
    float ratio = float(it->second.taken_count) / it->second.total;
    if (ratio > 0.9f)
      return true;
  }

  // All other branches use mybranch prediction
  return predict_branch(ip);
}

// Predict whether a branch will be taken or not
bool mybranch::predict_branch(champsim::address ip)
{
  std::optional<std::pair<std::size_t, TageEntry>> best_match;
  std::optional<std::pair<std::size_t, TageEntry>> alt_match;

  // Search for matching entries
  for (int i = NUM_TAGGED_TABLES - 1; i >= 0; --i) {
    auto [index, tag] = compute_index_and_tag(ip, i);
    const auto& entry = tagged_tables[i][index];

    if (entry.valid && entry.tag == tag) {
      if (!best_match.has_value())
        best_match = {i, entry};
      else if (!alt_match.has_value())
        alt_match = {i, entry};
    }
  }

  if (best_match.has_value()) {
    const auto& [best_idx, best_entry] = best_match.value();
    bool weak = best_entry.counter.value() == (best_entry.counter.maximum / 2);

    // If it's weakly predicted AND low confidence, fall back to alt or bimodal
    if (weak && best_entry.conf.value() < 2 && alt_selector[best_idx] > 0) {
      if (alt_match)
        return alt_match->second.counter.value() >= (alt_match->second.counter.maximum / 2);
    }

    // Otherwise, return main prediction
    return best_entry.counter.value() >= (best_entry.counter.maximum / 2);
  }

  // Fallback to bimodal
  return bimodal_table[bimodal_index(ip)].value() >= (bimodal_table[bimodal_index(ip)].maximum / 2);
}
// bool mybranch::predict_branch(champsim::address ip)
// {
//   std::optional<std::pair<std::size_t, TageEntry>> primary_match;
//   std::optional<std::pair<std::size_t, TageEntry>> alt_match;

//   // std::optional<bool> pred = std::nullopt;

//   // // Search for a matching entry (start from longest history to shortest)
//   // for (int i = NUM_TAGGED_TABLES - 1; i >= 0; --i) {
//   //   auto [index, tag] = compute_index_and_tag(ip, i);
//   //   const auto& entry = tagged_tables[i][index];

//   //   if (entry.valid && entry.tag == tag) {
//   //     return entry.counter.value() >= (entry.counter.maximum / 2); // Use prediction directly
//   //   }
//   // }

//   // Identify primary and alternate matching entries (to apply confidence rules)
//   for (int i = NUM_TAGGED_TABLES - 1; i >= 0; --i) {
//     auto [index, tag] = compute_index_and_tag(ip, i);
//     const auto& entry = tagged_tables[i][index];
//     if (entry.valid && entry.tag == tag) {
//       if (!primary_match.has_value())
//         primary_match = {i, entry}; // First match (longest)
//       else if (!alt_match.has_value())
//         alt_match = {i, entry}; // Next match (shorter)
//     }
//   }

//   if (primary_match.has_value()) {
//     const auto& [idx, entry] = primary_match.value();
//     bool weak = entry.counter.value() == entry.counter.maximum / 2;

//     // if (weak && alt_match.has_value() && alt_selector[idx] > 0) {
//     //   const auto& alt_entry = alt_match->second;
//     //   if (weak && alt_match && alt_selector[idx] > 0)
//     //     return alt_match->second.counter.value() >= (alt_match->second.counter.maximum / 2);
//     //   return entry.counter.value() >= (entry.counter.maximum / 2);
//     // }

//     // Cconfidence check
//     if (weak && entry.conf.value() < 2) {
//       if (alt_match)
//         return alt_match->second.counter.value() >= (alt_match->second.counter.maximum / 2);
//       else
//         return bimodal_table[bimodal_index(ip)].value() >= (bimodal_table[bimodal_index(ip)].maximum / 2);
//     }

//     return entry.counter.value() >= (entry.counter.maximum / 2);
//   }

//   // Fallback to bimodal if no matching entry
//   return bimodal_table[bimodal_index(ip)].value() >= (bimodal_table[bimodal_index(ip)].maximum / 2);
// }

// Update predictor with the actual outcome of the last branch
void mybranch::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
  // Step 1: Search for matching entry
  int hit_index = -1;
  std::size_t matched_table_idx = 0;

  for (int i = NUM_TAGGED_TABLES - 1; i >= 0; --i) {
    auto [index, tag] = compute_index_and_tag(ip, i);
    auto& entry = tagged_tables[i][index];

    if (entry.valid && entry.tag == tag) {
      hit_index = index;
      matched_table_idx = i;
      break;
    }
  }

  // Step 2: If found, train the matching entry by updating its counter
  if (hit_index != -1) {
    auto& entry = tagged_tables[matched_table_idx][hit_index];

    taken ? entry.counter += 1 : entry.counter -= 1;
    entry.useful = true;

    // Update confidence counter
    if ((entry.counter.value() >= (entry.counter.maximum / 2)) == taken)
      entry.conf += 1;
    else
      entry.conf -= 1;

    std::optional<std::pair<std::size_t, TageEntry>> alt_match;
    for (int i = matched_table_idx - 1; i >= 0; --i) {
      auto [alt_index, alt_tag] = compute_index_and_tag(ip, i);
      const auto& alt_entry = tagged_tables[i][alt_index];
      if (alt_entry.valid && alt_entry.tag == alt_tag) {
        alt_match = {i, alt_entry};
        break;
      }
    }

    if (entry.counter.value() == entry.counter.maximum / 2 && alt_match.has_value()) {
      const auto& [alt_idx, alt_entry] = alt_match.value();
      bool alt_prediction = alt_entry.counter.value() >= (alt_entry.counter.maximum / 2);

      if (alt_prediction != taken && alt_selector[matched_table_idx] > -8)
        --alt_selector[matched_table_idx];
      else if (alt_prediction == taken && alt_selector[matched_table_idx] < 8)
        ++alt_selector[matched_table_idx];
    }
  } else {
    // Step 3: No match — try inserting new entry into a random mybranch table
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

  // // Update Loop Count Predictor
  // auto& loop = loop_predictor[ip.to<std::size_t>()];
  // if (!taken) {
  //   if (loop.initialized && loop.current_iter > 0)
  //     loop.iteration_count = loop.current_iter;

  //   // Detect early loop exit and update LEB
  //   if (loop.initialized && loop.current_iter > 0) {
  //     loop_exit_buffer[leb_insert_ptr] = ip.to<std::size_t>();
  //     leb_insert_ptr = (leb_insert_ptr + 1) % LEB_SIZE;
  //   }

  //   loop.current_iter = 0;
  //   loop.initialized = true;
  // } else {
  //   loop.current_iter++;
  // }

  // Update loop bias tracker
  auto& bias = loop_bias[ip.to<std::size_t>()];
  bias.total++;
  if (taken)
    bias.taken_count++;

  if (branch_type == BRANCH_DIRECT_CALL)
    ras.push(branch_target);
  else if (branch_type == BRANCH_RETURN && !ras.empty())
    ras.pop();

  // Update the fallback bimodal predictor as well
  bimodal_table[bimodal_index(ip)] += taken ? 1 : -1;

  // Step 4: Update global history
  global_history <<= 1;         // Shift left (drop oldest bit)
  global_history.set(0, taken); // Insert newest outcome

  // Occasionally decay usefulness bits to prevent stale entries from staying forever
  if (++update_counter % 1024 == 0)
    age_usefulness_bits();

  if (update_counter % 8192 == 0) {
    for (auto& s : alt_selector)
      s /= 2;
  }
}

// Initialization
void mybranch::initialize_branch_predictor()
{
  // #error "INTENTIONAL BREAKPOINT - THIS FILE IS BEING COMPILED"
  global_history.reset();
  std::fill(alt_selector.begin(), alt_selector.end(), 0);
  while (!ras.empty())
    ras.pop();
  // loop_predictor.clear();
  // std::fill(loop_exit_buffer.begin(), loop_exit_buffer.end(), 0);
  // leb_insert_ptr = 0;
  loop_bias.clear();
}

// Final reporting
void mybranch::fini_branch_predictor()
{
  std::cout << "[mybranch Final] alt_selector: ";
  for (auto s : alt_selector)
    std::cout << s << " ";
  std::cout << std::endl;
}
