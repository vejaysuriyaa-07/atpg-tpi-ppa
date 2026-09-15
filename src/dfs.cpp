#include "../includes/core/dfs.hpp"
#include "../includes/Simulator.hpp"
#include "../includes/core/eds.hpp"
#include "../includes/utils.hpp"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <fstream>
#include <map>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

namespace logicsim {

std::string getFaultString(int nodeNum, char faultValue) {
  return std::to_string(nodeNum) + "@" + std::string(1, faultValue);
}

FaultId makeFaultId(unsigned nodeNum, char faultValue) {
  return (static_cast<FaultId>(nodeNum) << 1u) | (faultValue == '1' ? 1u : 0u);
}

int faultNode(FaultId id) { return static_cast<int>(id >> 1u); }

char faultValue(FaultId id) { return (id & 1u) ? '1' : '0'; }

std::map<int, std::vector<char>>
deductiveFaultSim_impl(Simulator &simulator, unsigned freq, bool atpg,
                       std::vector<std::vector<unsigned>> patternLines_arg) {
  simulator.simulationInit();
  simulator.circuitInit();
  std::vector<std::vector<unsigned>> patternLines;
  if (!atpg) {
    // ---------------------------------------------------------------------
    // Command argument parsing
    auto parsedArgs = parseCommandArgs(simulator.getCommandArgs());
    if (parsedArgs.empty()) {
      return {};
    }
    const std::string &patternPath = parsedArgs[0];
    const std::string &outputPath = parsedArgs[1];

    // ---------------------------------------------------------------------
    // Pattern file processing

    simulator.setPatternInputPath(patternPath);
    simulator.setPatternOutputPath(outputPath);

    std::ifstream patternFile(simulator.getPatternInputPath());
    if (!patternFile.is_open()) {
      printf("Unable to open pattern file: %s\n",
             simulator.getPatternInputPath().c_str());
      return {};
    }

    // Read full pattern file into memory: first line PI IDs, remaining lines
    // are ternary test vectors (0/1/x).

    std::string line;
    bool firstLine = true;
    while (std::getline(patternFile, line)) {
      auto lineFirst = line.find_first_not_of(" \t\r\n");
      if (lineFirst == std::string::npos) {
        continue;
      }
      auto lineLast = line.find_last_not_of(" \t\r\n");
      std::string trimmedLine =
          line.substr(lineFirst, lineLast - lineFirst + 1);

      std::vector<unsigned> values;
      std::istringstream row(trimmedLine);
      std::string token;
      if (firstLine) {
        while (std::getline(row, token, ',')) {
          token = trimCopy(token);
          if (token.empty()) {
            continue;
          }
          try {
            values.push_back(static_cast<unsigned>(std::stoul(token)));
          } catch (const std::exception &) {
            throw std::runtime_error("Malformed pattern header line: " +
                                     trimmedLine);
          }
        }
        firstLine = false;
      } else {
        while (std::getline(row, token, ',')) {
          token = trimCopy(token);
          if (token.empty()) {
            continue;
          }
          if (token.size() != 1) {
            throw std::runtime_error("Malformed pattern line: " + trimmedLine +
                                     "\n At token: " + token);
          }
          const char c = static_cast<char>(std::tolower(token[0]));
          if (c == '0') {
            values.push_back(0u);
          } else if (c == '1') {
            values.push_back(1u);
          } else if (c == 'x') {
            values.push_back(2u);
          } else {
            throw std::runtime_error(
                "Unsupported logic value in pattern line: " + trimmedLine);
          }
        }
      }
      if (!values.empty()) {
        patternLines.push_back(std::move(values));
      }
    }
    patternFile.close();
  } else {
    patternLines = std::move(patternLines_arg);
  }

  if (patternLines.empty()) {
    return {};
  }

  std::vector<unsigned> inputOrder = patternLines.front();
  std::map<unsigned, char> nodeValues;
  std::map<int, std::vector<char>> results;

  if (inputOrder.empty()) {
    throw std::runtime_error("Pattern header missing primary input IDs");
  }

  for (unsigned nodeNumber : inputOrder) {
    nodeValues[nodeNumber] = '\0';
  }

  auto &nodes = simulator.getNodes();
  std::vector<Node *> topoNodes;
  topoNodes.reserve(nodes.size());
  {
    std::vector<unsigned> indegree(nodes.size(), 0);
    std::queue<Node *> ready;
    for (auto &node : nodes) {
      indegree[node.getIndx()] = node.getFin();
      if (node.getFin() == 0) {
        ready.push(&node);
      }
    }
    while (!ready.empty()) {
      Node *curr = ready.front();
      ready.pop();
      topoNodes.push_back(curr);
      for (unsigned foutIdx = 0; foutIdx < curr->getFout(); ++foutIdx) {
        Node *downstream = curr->getDnodes()[foutIdx];
        if (!downstream) {
          continue;
        }
        unsigned idx = downstream->getIndx();
        if (indegree[idx] > 0 && --indegree[idx] == 0) {
          ready.push(downstream);
        }
      }
    }
    if (topoNodes.size() != nodes.size()) {
      topoNodes.clear();
      for (auto &node : nodes) {
        topoNodes.push_back(&node);
      }
      std::sort(topoNodes.begin(), topoNodes.end(),
                [](const Node *lhs, const Node *rhs) {
                  if (lhs->getLevel() != rhs->getLevel()) {
                    return lhs->getLevel() < rhs->getLevel();
                  }
                  return lhs->getIndx() < rhs->getIndx();
                });
    }
  }
  int step = 0;
  // One simulation per stored test vector (patternLines[0] is the PI header)
  for (std::size_t lineIdx = 1; lineIdx < patternLines.size(); ++lineIdx) {
    const auto &values = patternLines[lineIdx];
    if (values.size() != inputOrder.size()) {
      throw std::runtime_error("Pattern vector length does not match header");
    }
    for (std::size_t idx = 0; idx < inputOrder.size(); ++idx) {
      const unsigned v = values[idx];
      char valChar = '\0';
      if (v == 0u) {
        valChar = '0';
      } else if (v == 1u) {
        valChar = '1';
      } else if (v == 2u) {
        valChar = 'x';
      } else {
        throw std::runtime_error("Unsupported logic value in pattern vector");
      }
      nodeValues[inputOrder[idx]] = valChar;
    }
    for (Node *nodePtr : topoNodes) {
      nodePtr->clearFaultList();
    }

    eds(simulator, nodeValues);

    for (Node *nodePtr : topoNodes) {
      Node &node = *nodePtr;

      char localFaultValue =
          node.getValue() == LogicValue::ONE
              ? '0'
              : (node.getValue() == LogicValue::ZERO ? '1' : 'x');
      assert(localFaultValue != 'x');
      node.addFault(makeFaultId(node.getNum(), localFaultValue));

      switch (node.getType()) {
      case GateType::BRCH:
      case GateType::BUF:
      case GateType::NOT:
      case GateType::XOR:
      case GateType::XNOR:
        for (unsigned i = 0; i < node.getFin(); i++) {
          for (const auto &fault : node.getUnodes()[i]->getFaultList()) {
            node.addFault(fault);
          }
        }
        break;
      case GateType::AND:
        inferFaultList(node, 0, 0);
        break;
      case GateType::OR:
        inferFaultList(node, 1, 0);
        break;
      case GateType::NAND:
        inferFaultList(node, 0, 1);
        break;
      case GateType::NOR:
        inferFaultList(node, 1, 1);
        break;
      case GateType::IPT:
        break;
      }
    }
    for (auto &node : simulator.getPrimaryOutputs()) {
      for (FaultId fault : node->getFaultList()) {
        int nodeNum = faultNode(fault);
        char faultValueChar = faultValue(fault);
        if (std::find(results[nodeNum].begin(), results[nodeNum].end(),
                      faultValueChar) == results[nodeNum].end()) {
          results[nodeNum].push_back(faultValueChar);
        }
      }
    }

    simulator.circuitInit();
    step++;
    if (freq != 0 && step % freq == 0) {
      std::ofstream step_outfile(
          simulator.getPatternOutputPath().substr(
              0, simulator.getPatternOutputPath().find_last_of('.')) +
          "_step_" + std::to_string(step) + ".txt");
      for (auto &item : results) {
        for (const auto &fault : item.second) {
          step_outfile << item.first << "@" << fault << "\n";
        }
      }
      step_outfile.close();
    }
  }

  if (!atpg) {
    std::ofstream outfile(simulator.getPatternOutputPath());
    for (auto &item : results) {
      for (const auto &fault : item.second) {
        outfile << item.first << "@" << fault << "\n";
      }
    }
    outfile.close();
  }
  return results;
}

int inferFaultList(Node &node, unsigned c, unsigned i) {
  if (node.getFin() > 1) {
    if ((bool)node.getValue() == (bool)((1u - c) ^ i)) {
      // Inputs are all non controlling values, flipping one input will flip the
      // output.
      for (unsigned fi = 0; fi < node.getFin(); fi++) {
        node.addFaults(node.getUnodes()[fi]->getFaultList());
      }
      return 1;
    } else {
      std::vector<unsigned> controllingIndices;
      std::vector<unsigned> nonControllingIndices;
      for (unsigned fi = 0; fi < node.getFin(); fi++) {
        Node *unode = node.getUnodes()[fi];

        if (unode->getValue() == (LogicValue)c) {
          controllingIndices.push_back(fi);
        } else {
          nonControllingIndices.push_back(fi);
        }
      }
      if (controllingIndices.empty()) {
        return 1;
      }

      std::vector<FaultId> candidateFaults(
          node.getUnodes()[controllingIndices.front()]->getFaultList().begin(),
          node.getUnodes()[controllingIndices.front()]->getFaultList().end());

      for (std::size_t idx = 1; idx < controllingIndices.size(); ++idx) {
        const Node *fanin = node.getUnodes()[controllingIndices[idx]];
        std::vector<FaultId> next;
        for (const auto &fault : candidateFaults) {
          if (fanin->containsFault(fault)) {
            next.push_back(fault);
          }
        }
        candidateFaults.swap(next);
        if (candidateFaults.empty()) {
          break;
        }
      }

      if (candidateFaults.empty()) {
        return 1;
      }

      std::unordered_set<FaultId> blockingFaults;
      for (auto idx : nonControllingIndices) {
        for (const auto &fault : node.getUnodes()[idx]->getFaultList()) {
          blockingFaults.insert(fault);
        }
      }

      for (const auto &fault : candidateFaults) {
        if (!blockingFaults.count(fault)) {
          node.addFault(fault);
        }
      }
      return 1;
    }
  } else {
    for (unsigned fi = 0; fi < node.getFin(); fi++) {
      node.addFaults(node.getUnodes()[fi]->getFaultList());
    }
    return 1;
  }
}
} // namespace logicsim
