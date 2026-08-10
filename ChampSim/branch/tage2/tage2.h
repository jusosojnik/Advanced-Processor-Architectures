#ifndef BRANCH_TAGE_H
#define BRANCH_TAGE_H

#include <array>
#include <bitset>

#include "instruction.h"
#include "modules.h"
#include "msl/fwcounter.h"

// Number of tagged tables used in the tage2 predictor
static constexpr std::size_t NUM_TAGGED_TABLES = 6;

// Number of bits used for tags in each entry
static constexpr std::size_t TAG_BITS = 8;

// Number of bits for the saturating counters
static constexpr std::size_t COUNTER_BITS = 3;

// Number of entries in each tagged table
static constexpr std::size_t TABLE_SIZE = 512;

// History lengths for each tagged table
static constexpr std::array<std::size_t, NUM_TAGGED_TABLES> HISTORY_LENGTHS = {4, 8, 16, 32, 64, 128};

// Total number of bits in the global history register
static constexpr std::size_t GLOBAL_HISTORY_LENGTH = 128;

// A single entry in a tagged prediction table
struct TageEntry {
  champsim::msl::fwcounter<COUNTER_BITS> counter;
  uint8_t tag = 0;     // Tag used to distinguish entries with same index
  bool useful = false; // Indicates if the entry has been useful (for replacement)
  bool valid = false;  // Marks if the entry is in use (helps in matching)
};

struct tage2 : champsim::modules::branch_predictor {
  std::bitset<GLOBAL_HISTORY_LENGTH> global_history; // Global branch history (used in index/tag calculation)

  // Set of tagged tables: NUM_TAGGED_TABLES tables each with TABLE_SIZE entries
  std::array<std::array<TageEntry, TABLE_SIZE>, NUM_TAGGED_TABLES> tagged_tables;

  // Bimodal fallback table for simple predictions
  static constexpr std::size_t BIMODAL_TABLE_SIZE = 2048;
  std::array<champsim::msl::fwcounter<COUNTER_BITS>, BIMODAL_TABLE_SIZE> bimodal_table;

  std::size_t update_counter = 0; // Tracks updates to age out "useful" bits occasionally

  using branch_predictor::branch_predictor;

  // Computes the index and tag used to access entries in a specific tage2 table
  std::pair<std::size_t, uint8_t> compute_index_and_tag(champsim::address ip, std::size_t table_idx) const;

  // Predicts whether a branch will be taken or not based on the instruction pointer (IP)
  bool predict_branch(champsim::address ip);

  bool predict_branch(champsim::address ip, champsim::address predicted_target, bool always_taken, uint8_t branch_type);

  // Updates the predictor with the actual outcome of a branch instruction
  void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);

  // Computes index into the bimodal table
  std::size_t bimodal_index(champsim::address ip) const { return ip.to<std::size_t>() % BIMODAL_TABLE_SIZE; }

  // Ages the usefulness bits in all tagged tables to allow replacement of stale entries
  void age_usefulness_bits()
  {
    for (auto& table : tagged_tables) {
      for (auto& entry : table) {
        // Decay useful bit occasionally
        if (entry.useful)
          entry.useful = false;
      }
    }
  }
};

#endif
