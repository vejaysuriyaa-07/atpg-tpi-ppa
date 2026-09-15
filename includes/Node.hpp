#ifndef NODE_HPP
#define NODE_HPP

#include "types.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>
namespace logicsim {
using FaultId = std::uint32_t;
class Node {
private:
  using FaultId = std::uint64_t;
  unsigned indx; /* node index(from 0 to NumOfLine - 1 */
  unsigned num;  /* line number(May be different from indx */
  NodeType ntype;
  GateType gtype; /* gate type */
  unsigned fin;   /* number of fanins */
  unsigned fout;  /* number of fanouts */
  Node **unodes;  /* pointer to array of Fanin nodes */
  Node **dnodes;  /* pointer to array of Fanout nodes */
  int level;      /* level of the gate output */
  std::array<LogicValue, SystemWidth>
      value; /* 4 logic values of the node (1,0,z,u) */
  std::array<LogicValue, SystemWidth>
      latestScheduledValue; /* latest scheduled value for the node */
  std::array<unsigned, SystemWidth> f_and_mask;
  std::array<unsigned, SystemWidth> f_or_mask;
  std::unordered_set<FaultId> faultSet;
  
  // SCOAP (Sandia Controllability/Observability Analysis Program) attributes
  int cc0;  // Combinational Controllability to 0
  int cc1;  // Combinational Controllability to 1
  int co;   // Combinational Observability

public:
  Node();
  ~Node();
  void setIndx(unsigned i) { indx = i; }
  unsigned getIndx() const { return indx; }
  void setNum(unsigned n) { num = n; }
  unsigned getNum() const { return num; }
  void setNtype(NodeType t) { ntype = t; }
  NodeType getNtype() const { return ntype; }
  void setType(GateType t) { gtype = t; }
  GateType getType() const { return gtype; }
  void setFin(unsigned f) { fin = f; }
  unsigned getFin() const { return fin; }
  void setFout(unsigned f) { fout = f; }
  unsigned getFout() const { return fout; }
  void setUnodes(Node **u) { unodes = u; }
  Node **getUnodes() const { return unodes; }
  void setDnodes(Node **d) { dnodes = d; }
  Node **getDnodes() const { return dnodes; }
  void setLevel(int l) { level = l; }
  int getLevel() const { return level; }
  void setValue(LogicValue v, unsigned idx) { value[idx] = v; }
  LogicValue getValue(unsigned idx = 0) const { return value[idx]; }
  void setLatestScheduledValue(LogicValue v, unsigned idx) {
    latestScheduledValue[idx] = v;
  }
  LogicValue getLatestScheduledValue(unsigned idx) const {
    return latestScheduledValue[idx];
  }
  void setFAndMask(unsigned m, unsigned idx) { f_and_mask[idx] = m; }
  unsigned getFAndMask(unsigned idx) const { return f_and_mask[idx]; }
  void setFOrMask(unsigned m, unsigned idx) { f_or_mask[idx] = m; }
  unsigned getFOrMask(unsigned idx) const { return f_or_mask[idx]; }
  const std::unordered_set<FaultId> &getFaultList() const { return faultSet; }
  bool containsFault(FaultId id) const { return faultSet.find(id) != faultSet.end(); }
  void addFault(FaultId id) { faultSet.insert(id); }
  void addFaults(const std::unordered_set<FaultId> &faults) {
    faultSet.reserve(faultSet.size() + faults.size());
    faultSet.insert(faults.begin(), faults.end());
  }
  void clearFaultList() { faultSet.clear(); }
  void setFaultList(const std::vector<FaultId> &fl) {
    faultSet.clear();
    faultSet.insert(fl.begin(), fl.end());
  }
  
  // SCOAP getters and setters
  void setCC0(int val) { cc0 = val; }
  int getCC0() const { return cc0; }
  void setCC1(int val) { cc1 = val; }
  int getCC1() const { return cc1; }
  void setCO(int val) { co = val; }
  int getCO() const { return co; }
  
  void clear();
};
} // namespace logicsim
#endif
