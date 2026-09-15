#include "../includes/core/eds.hpp"
#include "../includes/Simulator.hpp"
#include "../includes/utils.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace logicsim {

std::map<int, char> eds(Simulator &simulator,
                        std::map<unsigned, char> &nodeValues, unsigned idx) {
  auto &eventTable = simulator.getEventTable();
  auto &nodes = simulator.getNodes();
  auto &primaryInputs = simulator.getPrimaryInputs();
  auto &primaryOutputs = simulator.getPrimaryOutputs();
  std::map<int, char> results;

  // Build a fast lookup for primary input node number -> Node*
  // This avoids an O(N) find for every pattern value insertion.
  std::unordered_map<int, Node *> piMap;
  piMap.reserve(primaryInputs.size() * 2 + 1);
  for (const Node *n : primaryInputs) {
    if (n)
      piMap[(int)n->getNum()] = const_cast<Node *>(n);
  }

  // Insert events for primary inputs at time 0
  for (const auto &x : nodeValues) {
    LogicValue lv = LogicValue::UNDEF;
    if (x.second == '0') {
      lv = LogicValue::ZERO;
    } else if (x.second == '1') {
      lv = LogicValue::ONE;
    } else if (x.second == 'x') {
      lv = LogicValue::UNDEF;
    } else {
      printf("Unsupported logic value %d for node %u.\n", x.second, x.first);
    }
    auto pit = piMap.find(x.first);

    if (pit == piMap.end() || pit->second == nullptr) {
      printf("Primary input node %u not found; pattern ignored.\n", x.first);
      continue;
    }
    eventTable.insert(pit->second, lv, 0);
  }
  // Requirements: eventTable initialized with some starting events.
  eds_stepping(simulator, idx);

  for (int i = 0; i < simulator.getNPO(); i++) {
    results[primaryOutputs[i]->getNum()] =
        primaryOutputs[i]->getValue(idx) == ONE
            ? '1'
            : (primaryOutputs[i]->getValue(idx) == ZERO
                   ? '0'
                   : (primaryOutputs[i]->getValue(idx) == HIGHZ ? 'z' : 'x'));
  }
  return results;
}

void eds_stepping(Simulator &simulator, unsigned idx) {
  auto &eventTable = simulator.getEventTable();
  unsigned time = 0;
  while (!eventTable.empty()) {
    // eventTable.printTable();
    std::vector<Event> events = eventTable.extract(time);
    for (const auto &event : events) {
      Node *node = event.getTargetNode();
      LogicValue newValue = event.getNewValue();
      if (node->getValue(idx) != newValue) {
        // Set new values
        node->setValue(newValue, idx);
        // Check fanout and schedule new events
        for (unsigned i = 0; i < node->getFout(); ++i) {
          Node *fanoutNode = node->getDnodes()[i];
          // Set lsv to current value before evaluation
          fanoutNode->setLatestScheduledValue(fanoutNode->getValue(idx), idx);
          LogicValue newVal = evaluate(fanoutNode, idx);
          if (newVal != fanoutNode->getLatestScheduledValue(idx)) {
            // Schedule new event if new value differs from lsv
            unsigned delay =
                fanoutNode->getType() == BRCH ? 1 : 2; // Example delay logic
            eventTable.insert(fanoutNode, newVal, time + delay);
            // Update lsv
            fanoutNode->setLatestScheduledValue(newVal, idx);
          }
        }
      }
    }
    time++;
    eventTable.prune(time);
  }
}

int eventDrivenSim_impl(Simulator &simulator) {
  simulator.simulationInit();
  simulator.circuitInit();
  int nPO = simulator.getNPO();
  int nPI = simulator.getNPI();

  // ---------------------------------------------------------------------
  // Command argument parsing
  auto parsedArgs = parseCommandArgs(simulator.getCommandArgs());
  if (parsedArgs.empty()) {
    return 0;
  }
  const std::string patternPath = parsedArgs[0];
  const std::string outputPath = parsedArgs[1];

  // ---------------------------------------------------------------------
  // Pattern file processing

  simulator.setPatternInputPath(patternPath);
  simulator.setPatternOutputPath(outputPath);

  std::ifstream patternFile(simulator.getPatternInputPath());
  if (!patternFile.is_open()) {
    printf("Unable to open pattern file: %s\n",
           simulator.getPatternInputPath().c_str());
    return 0;
  }

  std::string line;
  unsigned lineNum = 0;
  std::map<unsigned, char> nodeValues;
  std::map<int, std::vector<char>> results;

  // One simulation per line/loop
  while (std::getline(patternFile, line)) {
    auto lineFirst = line.find_first_not_of(" \t\r\n");
    if (lineFirst == std::string::npos) {
      continue;
    }
    auto lineLast = line.find_last_not_of(" \t\r\n");
    line = line.substr(lineFirst, lineLast - lineFirst + 1);

    std::istringstream row(line);
    unsigned nodeNumber = 0;
    char separator = '\0';
    char value = '\0';
    // First line initialization for PIs
    if (lineNum == 0) {
      while (row >> nodeNumber) {
        nodeValues[nodeNumber] = '\0';
        if (row.peek() == EOF) {
          break;
        } else if (!(row >> separator) || separator != ',') {
          throw std::runtime_error("Malformed pattern line: " + line);
        }
      }
      lineNum++;
      continue;
    } else {
      // Prepare one line of input vector
      for (const auto &x : nodeValues) {
        row >> value;
        nodeValues[x.first] = value;
        if (row.peek() == EOF) {
          break;
        } else if (!(row >> separator) || separator != ',') {
          throw std::runtime_error("Malformed pattern line: " + line);
        }
      }
    }

    auto iterationResults = eds(simulator, nodeValues);

    for (auto &entry : iterationResults) {
      auto &dest = results[entry.first];
      dest.push_back(entry.second);
    }
    simulator.circuitInit();
    lineNum++;
  }
  nodeValues.clear();
  patternFile.close();

  if (simulator.getPatternOutputPath().empty() || results.empty()) {
    return 0;
  }

  std::ofstream outfile;
  outfile.open(simulator.getPatternOutputPath());

  bool first = true;
  for (const auto &x : results) {
    if (!first)
      outfile << ',';
    first = false;
    outfile << x.first;
  }
  outfile << '\n';

  std::size_t resultSize = results.begin()->second.size();
  for (std::size_t i = 0; i < resultSize; ++i) {
    bool first = true;
    for (const auto &x : results) {
      if (!first)
        outfile << ',';
      first = false;
      outfile << x.second[i];
    }
    outfile << '\n';
  }
  outfile.close();
  results.clear();
  return 1;
}

LogicValue evaluate(Node *node, unsigned idx) {
  bool andmask = (bool)(node->getFAndMask(idx));
  bool ormask = (bool)(node->getFOrMask(idx));
  switch (node->getType()) {
  case NOT: {
    if (node->getFin() != 1) {
      return UNDEF;
    }
    LogicValue finValue = node->getUnodes()[0]->getValue(idx);
    if (finValue == ZERO) {
      bool a = true;
      bool s = (a && andmask) || ormask;
      return s ? ONE : ZERO;
    } else if (finValue == ONE) {
      bool a = false;
      bool s = (a && andmask) || ormask;
      return s ? ONE : ZERO;
    } else if (finValue == D) {
      return SD;
    } else if (finValue == SD) {
      return D;
    } else {
      return UNDEF;
    }
  }
  case AND:
    return evaluate(node, 0, 0, idx);
    break;
  case NAND:
    return evaluate(node, 0, 1, idx);
    break;
  case OR:
    return evaluate(node, 1, 0, idx);
    break;
  case NOR:
    return evaluate(node, 1, 1, idx);
    break;
  case XOR: {
    unsigned countOnes = 0;
    bool unknown = false;
    std::vector<LogicValue> DValues;
    for (unsigned i = 0; i < node->getFin(); ++i) {
      LogicValue finValue = node->getUnodes()[i]->getValue(idx);
      if (finValue == D || finValue == SD) {
        DValues.push_back(finValue);
        continue;
      }
      if (finValue == ONE) {
        countOnes++;
      } else if (finValue == UNDEF || finValue == HIGHZ) {
        unknown = true;
      }
    }
    if (unknown) {
      return UNDEF;
    }
    unsigned normalOnes = countOnes;
    unsigned faultOnes = countOnes;
    if (!DValues.empty()) {
      for (const auto &DValue : DValues) {
        if (DValue == D) {
          normalOnes++;
        } else if (DValue == SD) {
          faultOnes++;
        }
      }
      bool normalA = normalOnes % 2 == 1;
      bool faultA = faultOnes % 2 == 1;
      return (normalA && !faultA) ? D : (!normalA && faultA) ? SD : (normalA == faultA) ? (normalA ? ONE : ZERO) : UNDEF;
    }
    bool a = countOnes % 2 == 1;
    bool s = (a && andmask) || ormask;
    return s ? ONE : ZERO;

  } break;
  case XNOR: {
    unsigned countOnes = 0;
    bool unknown = false;
    std::vector<LogicValue> DValues;
    for (unsigned i = 0; i < node->getFin(); ++i) {
      LogicValue finValue = node->getUnodes()[i]->getValue(idx);
      if (finValue == D || finValue == SD) {
        DValues.push_back(finValue);
        continue;
      }
      if (finValue == ONE) {
        countOnes++;
      } else if (finValue == UNDEF || finValue == HIGHZ) {
        unknown = true;
      }
    }
    if (unknown) {
      return UNDEF;
    }
    unsigned normalOnes = countOnes;
    unsigned faultOnes = countOnes;
    if (!DValues.empty()) {
      for (const auto &DValue : DValues) {
        if (DValue == D) {
          normalOnes++;
        } else if (DValue == SD) {
          faultOnes++;
        }
      }
      bool normalA = normalOnes % 2 == 0;
      bool faultA = faultOnes % 2 == 0;
      return (normalA && !faultA) ? D : (!normalA && faultA) ? SD : (normalA == faultA) ? (normalA ? ONE : ZERO) : UNDEF;
    }
    bool a = countOnes % 2 == 0;
    bool s = (a && andmask) || ormask;
    return s ? ONE : ZERO;
  } break;
  case BUF:
  case BRCH: {
    if (node->getFin() != 1) {
      return UNDEF;
    }
    LogicValue finValue = node->getUnodes()[0]->getValue(idx);
    if (finValue == UNDEF || finValue == HIGHZ) {
      return UNDEF;
    }
    if (finValue == D || finValue == SD) {
      return finValue;
    }
    bool a = finValue == ONE ? true : false;
    bool s = (a && andmask) || ormask;
    return s ? ONE : ZERO;
  } break;
  case IPT: {
    LogicValue val = node->getValue(idx);
    if (val == ONE || val == ZERO) {
      bool a = (val == ONE);
      bool s = (a && andmask) || ormask;
      return s ? ONE : ZERO;
    }
    if (ormask)
      return ONE;
    if (!andmask)
      return ZERO;
    return val;
  }
    break;
  default:
    return UNDEF;
  }
}
LogicValue evaluate(Node *node, unsigned c, unsigned i, unsigned idx) {
  bool andmask = (bool)(node->getFAndMask(idx));
  bool ormask = (bool)(node->getFOrMask(idx));
  bool unknown = false;
  std::vector<LogicValue> DValues;
  for (unsigned j = 0; j < node->getFin(); ++j) {
    LogicValue finValue = node->getUnodes()[j]->getValue(idx);
    if (finValue == D || finValue == SD) {
      DValues.push_back(finValue);
    } else if (finValue == UNDEF || finValue == HIGHZ) {
      unknown = true;
    } else if (finValue == c) {
      bool a = (c ^ i);
      bool s = (a && andmask) || ormask;
      return s ? ONE : ZERO;
    }
  }
  // Other inputs are non-controlling values, decide based on D/D'
  LogicValue normalRes = UNDEF;
  LogicValue faultRes = UNDEF;
  if (!DValues.empty()) {
    for (const auto &DValue : DValues) {
      unsigned normalVal = (DValue == D) ? ONE : ZERO;
      unsigned faultVal = (DValue == D) ? ZERO : ONE;
      // Either normal or fault side can determine the output
      if (normalVal == c) {
        bool a = (c ^ i);
        normalRes = a ? ONE : ZERO;
      } else if (faultVal == c) {
        bool a = (c ^ i);
        faultRes = a ? ONE : ZERO;
      }
    }
    // If one of the sides is still UNDEF, and no unknown values on other inputs, then this side is non-controlling value
    if (!unknown) {
      if (normalRes == UNDEF) normalRes = (i ^ (1u - c)) ? ONE : ZERO;
      if (faultRes == UNDEF) faultRes = (i ^ (1u - c)) ? ONE : ZERO;
      if (normalRes == ONE && faultRes == ZERO) {
        return D;
      } else if (normalRes == ZERO && faultRes == ONE) {
        return SD;
      } else if (normalRes == faultRes) {
        return normalRes;
      }
    } else {
      return UNDEF;
    }
  }
  
  if (unknown) {
    return UNDEF;
  } else {
    bool a = (i ^ (1u - c));
    bool s = (a && andmask) || ormask;
    return s ? ONE : ZERO;
  }
}
} // namespace logicsim
