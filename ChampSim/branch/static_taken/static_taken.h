#ifndef BRANCH_STATIC_TAKEN_H
#define BRANCH_STATIC_TAKEN_H

#include <cstdint>

#include "address.h"

class O3_CPU;

class static_taken
{
public:
  static_taken(O3_CPU*) {};

  [[nodiscard]] bool predict_branch(champsim::address ip);
  void last_branch_result(champsim::address ip, champsim::address target, bool taken, std::uint8_t branch_type);
};

#endif // BRANCH_STATIC_TAKEN_H