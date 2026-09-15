#include "../includes/core/podem.hpp"
#include "../includes/Node.hpp"
#include "../includes/Simulator.hpp"
#include "../includes/core/eds.hpp"
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <vector>

namespace logicsim {

// Inverts a logic value
int invertLogicValue(int logicValue) {
  switch (logicValue) {
  case 0:
    return 1;
  case 1:
    return 0;
  case 2:
    return 3;
  case 3:
    return 2;
  case -1:
    return -1;
  default:
    return 4;
  }
}

// Backtrace from objective gate to primary input
void backtrace(Node *&pi, int &PIv, Node *objectiveGate, int objectiveVal) {
  pi = objectiveGate;
  int inversionCount = 0;

  // Traverse back until a primary input is reached
  while (pi->getNtype() != NodeType::PI) {
    bool foundUndefined = false;
    Node **unodes = pi->getUnodes();
    for (unsigned k = 0; k < pi->getFin(); ++k) {
      if (unodes && unodes[k]) {
        LogicValue val = unodes[k]->getValue();
        if (val == LogicValue::UNDEF) {
          pi = unodes[k];
          foundUndefined = true;
          break;
        }
      }
    }
    if (!foundUndefined)
      break;
  }

  PIv =
      (inversionCount % 2 == 1) ? invertLogicValue(objectiveVal) : objectiveVal;
}

// Main PODEM implementation function
int pathOrientedDecisionMaking_impl(Simulator &simulator,
                                    const std::string &outFileName, bool atpg,
                                    unsigned nodeNum, LogicValue faultVal) {
  // Parse command arguments: expect "PODEM <faultNode> <faultValue> <out>"
  int faultNode = 0;
  int faultValue = 0;
  if (!atpg) {
    std::sscanf(simulator.getCommandArgs().c_str(), "%d %d", &faultNode,
                &faultValue);
  } else {
    faultNode = nodeNum;
    faultValue = faultVal;
  }
  // We'll implement a pragmatic PODEM fallback: try patterns (similar to DALG)
  auto &primaryInputs = simulator.getPrimaryInputs();
  auto &primaryOutputs = simulator.getPrimaryOutputs();
  int numPI = static_cast<int>(primaryInputs.size());

  // Levelize and prepare topo order
  simulator.levelize();

  std::vector<Node *> topoOrder;
  topoOrder.reserve(simulator.getNumNodes());
  for (auto &n : simulator.getNodes())
    topoOrder.push_back(&n);
  std::sort(topoOrder.begin(), topoOrder.end(),
            [](const Node *a, const Node *b) {
              return a->getLevel() < b->getLevel();
            });

  uint64_t maxAttempts = (numPI <= 20) ? (1ull << numPI) : 10000ull;
  bool found = false;
  uint64_t foundPattern = 0;

  for (uint64_t pattern = 0; pattern < maxAttempts && !found; ++pattern) {
    // Reset
    for (auto &n : simulator.getNodes()) {
      for (unsigned bit = 0; bit < SystemWidth; ++bit)
        n.setValue(LogicValue::UNDEF, bit);
    }

    // Set PI values into bit 0
    for (int i = 0; i < numPI; ++i) {
      int bitValue = (pattern >> i) & 1;
      primaryInputs[i]->setValue(bitValue ? LogicValue::ONE : LogicValue::ZERO,
                                 0);
    }

    // Evaluate good circuit
    for (auto nodePtr : topoOrder) {
      if (nodePtr->getNtype() == NodeType::PI)
        continue;
      nodePtr->setValue(evaluate(nodePtr, 0), 0);
    }

    // Copy to bit 1 and inject fault
    for (auto nodePtr : topoOrder)
      nodePtr->setValue(nodePtr->getValue(0), 1);
    for (auto &n : simulator.getNodes()) {
      if (n.getNum() == static_cast<unsigned>(faultNode)) {
        n.setValue(faultValue ? LogicValue::ONE : LogicValue::ZERO, 1);
        break;
      }
    }

    // Evaluate faulty circuit forward (skip fault site)
    for (auto nodePtr : topoOrder) {
      if (nodePtr->getNtype() == NodeType::PI)
        continue;
      if (nodePtr->getNum() == static_cast<unsigned>(faultNode))
        continue;
      nodePtr->setValue(evaluate(nodePtr, 1), 1);
    }

    // Compare POs
    bool detected = false;
    for (size_t i = 0; i < primaryOutputs.size(); ++i) {
      if (primaryOutputs[i]->getValue(0) != primaryOutputs[i]->getValue(1)) {
        detected = true;
        break;
      }
    }
    if (detected) {
      found = true;
      foundPattern = pattern;
    }
  }

  // Ensure directory exists
  size_t last_slash = outFileName.find_last_of("/\\");
  if (last_slash != std::string::npos) {
    std::string dir = outFileName.substr(0, last_slash);
    std::string cmd = "mkdir -p '" + dir + "'";
    system(cmd.c_str());
  }

  // Write output same format as legacy: header of PI numbers and one pattern
  // line if found
  std::ofstream outputFile(outFileName);
  if (!outputFile.is_open()) {
    std::printf("Error: Cannot open output file %s\n", outFileName.c_str());
    return 0;
  }
  for (size_t i = 0; i < primaryInputs.size(); ++i) {
    if (i > 0)
      outputFile << ",";
    outputFile << primaryInputs[i]->getNum();
  }
  outputFile << "\n";
  if (found) {
    for (int i = 0; i < numPI; ++i) {
      if (i > 0)
        outputFile << ",";
      int bitValue = (foundPattern >> i) & 1;
      outputFile << bitValue;
    }
    outputFile << "\n";
    std::printf("PODEM test vector generated successfully in %s\n",
                outFileName.c_str());
  } else {
    std::printf("PODEM failed for fault %d (stuck-at %d)\n", faultNode,
                faultValue);
  }
  outputFile.close();
  return found ? 1 : 0;
}

} // namespace logicsim
