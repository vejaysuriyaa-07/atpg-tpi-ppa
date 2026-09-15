#include "../includes/Node.hpp"
#include <cstdlib>

namespace logicsim {
Node::Node() {
  indx = 0;
  num = 0;
  ntype = NodeType::PI;
  gtype = GateType::IPT;
  fin = 0;
  fout = 0;
  unodes = nullptr;
  dnodes = nullptr;
  level = -1;
  value.fill(LogicValue::UNDEF);
  latestScheduledValue.fill(LogicValue::UNDEF);
  f_and_mask.fill(1);
  f_or_mask.fill(0);
  // SCOAP values initialized to -1 (not computed)
  cc0 = -1;
  cc1 = -1;
  co = -1;
}
Node::~Node() { clear(); }
void Node::clear() {
  delete[] unodes;
  delete[] dnodes;
  unodes = nullptr;
  dnodes = nullptr;
  fin = 0;
  fout = 0;
  level = -1;
  faultSet.clear();
}
} // namespace logicsim
