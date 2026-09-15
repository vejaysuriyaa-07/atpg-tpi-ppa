#pragma once
#include "../types.hpp"
#include <string>
#include <vector>

namespace logicsim {
class Simulator;
class Node;
int dAlgorithm_impl(Simulator &simulatorconst, bool atpg = false,
                    unsigned nodeNum_arg = 0,
                    LogicValue faultValue_arg = LogicValue::UNDEF,
                    unsigned DF_arg = 0, unsigned JF_arg = 0);
bool dalg(Simulator &simulator, std::vector<Node *> &nodes,
          std::vector<Node *> &extraNodes, unsigned DF_arg, unsigned JF_arg);
bool implyAndCheck(Simulator &simulator, Node &node,
                   std::vector<Node *> &DFrontier,
                   std::vector<Node *> &JFrontier);
bool justifyJFrontier(Simulator &simulator, std::vector<Node *> &JFrontier,
                      std::size_t index);

} // namespace logicsim
