#pragma once

#include "../Node.hpp"
#include <map>

namespace logicsim {
class Simulator;
// Run: DFS <tp-file> <out-file>
std::map<int, std::vector<char>> deductiveFaultSim_impl(Simulator &simulator, unsigned freq = 0, bool atpg = false, std::vector<std::vector<unsigned>> patternLines_arg = {});
std::string getFaultString(int nodeNum, char faultValue);
int inferFaultList(Node &node, unsigned c, unsigned i);
} // namespace logicsim
