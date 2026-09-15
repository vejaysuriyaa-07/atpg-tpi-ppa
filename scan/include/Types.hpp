#pragma once
#include <string>
#include <vector>

namespace scan {

enum GateType { G_AND, G_OR, G_NAND, G_NOR, G_NOT, G_BUFF, G_XOR, G_XNOR, G_SOURCE };
enum LogicVal { ZERO = 0, ONE = 1, UNKNOWN = 2 };

struct Gate {
    std::string name;
    GateType gtype;
    std::vector<std::string> inputs;
    LogicVal value;
    int level;
};

struct DFF {
    std::string qName;
    std::string dName;
};

struct Fault {
    std::string gateName;
    int stuckAt;
};

} // namespace scan
