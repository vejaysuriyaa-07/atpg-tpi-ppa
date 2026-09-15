#pragma once

#include "../types.hpp"
#include <string>

namespace logicsim {
class Simulator;

// Path-Oriented Decision Making (PODEM) algorithm implementation
// Generates test vectors for fault detection
// Usage: PODEM <fault_node> <fault_value> <output_file>
int pathOrientedDecisionMaking_impl(Simulator &simulator,
                                    const std::string &outFileName,
                                    bool atpg = false, unsigned nodeNum = 0,
                                    LogicValue faultVal = UNDEF);

} // namespace logicsim
