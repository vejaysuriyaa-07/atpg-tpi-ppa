#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace bist {

enum NodeType { GATE = 0, PI = 1, FB = 2, PO = 3 };
enum GateType { IPT = 0, BRCH = 1, XOR = 2, OR = 3, NOR = 4, NOT = 5, NAND = 6, AND = 7, XNOR = 8, BUF = 9 };
enum LogicVal { ZERO = 0, ONE = 1, UNKNOWN = 2 };

struct Gate {
    int id;
    NodeType ntype;
    GateType gtype;
    int fanout;
    int fanin;
    std::vector<int> inputs;
    LogicVal value;
    int level;
};

struct Fault {
    int gateId;
    int stuckAt;
};

} // namespace bist
