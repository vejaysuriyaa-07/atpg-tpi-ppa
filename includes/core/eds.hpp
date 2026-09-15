#pragma once

#include "../Node.hpp"
#include "../types.hpp"
#include <map>
namespace logicsim {
class Simulator;
std::map<int, char> eds(Simulator &simulator,
                        std::map<unsigned, char> &nodeValues, unsigned idx = 0);
void eds_stepping(Simulator &simulator, unsigned idx);
int eventDrivenSim_impl(Simulator &simulator);
LogicValue evaluate(Node *node, unsigned idx);
LogicValue evaluate(Node *node, unsigned c, unsigned i, unsigned idx);
} // namespace logicsim
