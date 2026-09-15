#include "../includes/core/ppa.hpp"
#include <fstream>
#include <sstream>

namespace logicsim {

bool PpaModel::loadFromFile(const std::string &path, std::string &error) {
  std::ifstream in(path);
  if (!in.is_open()) {
    error = "Cannot open PPA model file: " + path;
    return false;
  }

  std::string line;
  int lineNo = 0;
  while (std::getline(in, line)) {
    ++lineNo;
    if (line.empty() || line[0] == '#')
      continue;
    std::istringstream ss(line);
    std::string key;
    double value = 0.0;
    if (!(ss >> key >> value)) {
      error = "Malformed entry at line " + std::to_string(lineNo);
      return false;
    }

    if (key == "inv_area") invArea = value;
    else if (key == "buf_area") bufArea = value;
    else if (key == "nand2_area") nand2Area = value;
    else if (key == "nor2_area") nor2Area = value;
    else if (key == "and2_area") and2Area = value;
    else if (key == "or2_area") or2Area = value;
    else if (key == "xor2_area") xor2Area = value;
    else if (key == "inv_delay") invDelay = value;
    else if (key == "buf_delay") bufDelay = value;
    else if (key == "nand2_delay") nand2Delay = value;
    else if (key == "nor2_delay") nor2Delay = value;
    else if (key == "and2_delay") and2Delay = value;
    else if (key == "or2_delay") or2Delay = value;
    else if (key == "xor2_delay") xor2Delay = value;
    else {
      error = "Unknown key '" + key + "' at line " + std::to_string(lineNo);
      return false;
    }
  }

  source = "file:" + path;
  return true;
}

// A k-input gate costs k-1 two-input cells. Branches and input terminals are
// wiring, not logic, so they cost nothing.
static double widen(double base2, unsigned fanin) {
  if (fanin <= 2)
    return base2;
  return base2 * static_cast<double>(fanin - 1);
}

double PpaModel::areaOf(GateType type, unsigned fanin) const {
  switch (type) {
  case IPT:
  case BRCH:
    return 0.0;
  case NOT:
    return invArea;
  case BUF:
    return bufArea;
  case NAND:
    return widen(nand2Area, fanin);
  case NOR:
    return widen(nor2Area, fanin);
  case AND:
    return widen(and2Area, fanin);
  case OR:
    return widen(or2Area, fanin);
  case XOR:
  case XNOR:
    return widen(xor2Area, fanin);
  default:
    return 0.0;
  }
}

double PpaModel::delayOf(GateType type, unsigned fanin) const {
  // Wide gates decompose into a tree, so delay grows with log2 of the fanin
  // rather than linearly.
  double stages = 1.0;
  for (unsigned f = fanin; f > 2; f = (f + 1) / 2)
    stages += 1.0;

  switch (type) {
  case IPT:
  case BRCH:
    return 0.0;
  case NOT:
    return invDelay;
  case BUF:
    return bufDelay;
  case NAND:
    return nand2Delay * stages;
  case NOR:
    return nor2Delay * stages;
  case AND:
    return and2Delay * stages;
  case OR:
    return or2Delay * stages;
  case XOR:
  case XNOR:
    return xor2Delay * stages;
  default:
    return 0.0;
  }
}

} // namespace logicsim
