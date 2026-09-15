#pragma once

#include <cstddef>
#include <functional>

namespace logicsim {

const unsigned MAXNAME = 31;  /* File name size */
const unsigned MAXLINE = 512; /* Input buffer size */
// Number of command slots.
const unsigned NUMFUNCS = 18;
constexpr std::size_t SystemWidth = (sizeof(void *) == 8) ? 64 : 32;

enum INST { READ, PC, HELP, QUIT };
enum State { EXEC, CKTLD };
enum NodeType { GATE, PI, FB, PO };
enum GateType { IPT, BRCH, XOR, OR, NOR, NOT, NAND, AND, XNOR, BUF };
enum LogicValue { ZERO, ONE, UNDEF, HIGHZ, D, SD };

inline const char *logicValueName(LogicValue value) {
  switch (value) {
  case ZERO:
    return "ZERO";
  case ONE:
    return "ONE";
  case UNDEF:
    return "UNDEF";
  case HIGHZ:
    return "HIGHZ";
  case D:
    return "D";
  case SD:
    return "SD";
  default:
    return "UNKNOWN";
  }
}

struct Fault {
  int node_num;
  int fault_value;

  bool operator<(const Fault &other) const {
    if (node_num != other.node_num)
      return node_num < other.node_num;
    return fault_value < other.fault_value;
  }

  bool operator==(const Fault &other) const {
    return node_num == other.node_num && fault_value == other.fault_value;
  }
};

} // namespace logicsim

namespace std {
template <> struct hash<logicsim::Fault> {
  size_t operator()(const logicsim::Fault &f) const noexcept {
    auto h1 = std::hash<int>{}(f.node_num);
    auto h2 = std::hash<int>{}(f.fault_value);
    return h1 ^ (h2 << 1); // Combine the hashes
  }
};
} // namespace std
