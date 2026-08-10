#include "static_taken.h"

bool static_taken::predict_branch(champsim::address /*ip*/)
{
  return true; // Always predict taken
}

void static_taken::last_branch_result(champsim::address, champsim::address, bool, uint8_t)
{
  // No update needed
}