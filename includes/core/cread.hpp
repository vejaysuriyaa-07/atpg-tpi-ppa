#pragma once

#include "../types.hpp"
#include <iosfwd>
#include <string>
#include <vector>

namespace logicsim {
class Simulator;
class Node;

struct CircuitLayout {
  int numNodes = 0;
  int nPI = 0;
  int nPO = 0;
  int maxId = -1;
};

struct ParsedNode {
  int nodeId = 0;
  NodeType nodeType = NodeType::GATE;
  GateType gateType = GateType::IPT;
  int fin = 0;
  int fout = 0;
  std::vector<int> faninIds;
};

struct CircuitParseResult {
  CircuitLayout layout;
  std::vector<int> idToIndex;
  std::vector<ParsedNode> nodes;
};

CircuitLayout scanLayout(std::istream &input);
bool parseCircuit(std::istream &input, const CircuitLayout &layout,
                  CircuitParseResult &result, std::string &error);
bool materializeCircuit(const CircuitParseResult &parsed,
                        std::vector<Node> &nodes,
                        std::vector<Node *> &primaryInputs,
                        std::vector<Node *> &primaryOutputs,
                        std::string &error);
void linkFanouts(std::vector<Node> &nodes);
int cread_impl(Simulator &simulator);
} // namespace logicsim
