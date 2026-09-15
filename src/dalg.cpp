#include "../includes/core/dalg.hpp"
#include "../includes/Simulator.hpp"
#include "../includes/core/eds.hpp"
#include "../includes/core/scoap.hpp"
#include "../includes/utils.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace logicsim {

namespace {

// Simple guard to avoid runaway recursion/search in DALG on very
// hard/untestable faults. The limit is generous enough for normal cases but
// prevents the solver from effectively running forever. Tweak this limit if
// DALG needs to search deeper; current value is chosen to stop runaway cases on
// large ISCAS'85 circuits such as c6288.
constexpr std::size_t kMaxDalgCalls = 5000;
constexpr std::size_t kMaxTracePrints = 200;
std::size_t g_dalgCallCount = 0;
bool g_dalgLimitHit = false;
std::size_t g_dalgTraceCount = 0;

LogicValue toGoodLogicValue(LogicValue value) {
  if (value == D)
    return ONE;
  if (value == SD)
    return ZERO;
  return value;
}

void restoreSnapshot(std::vector<Node> &nodes,
                     const std::vector<LogicValue> &snapshot) {
  for (std::size_t i = 0; i < snapshot.size() && i < nodes.size(); ++i) {
    nodes[i].setValue(snapshot[i], 0);
  }
}

bool justifyNodeFanins(Simulator &simulator, Node *jfNode,
                       std::vector<Node *> &undefFanins,
                       std::size_t faninIndex) {
  const LogicValue targetGood = toGoodLogicValue(jfNode->getValue(0));
  if (targetGood == UNDEF) {
    return false;
  }

  LogicValue evalVal = evaluate(jfNode, 0);
  LogicValue evalGood = toGoodLogicValue(evalVal);
  if (evalVal != UNDEF) {
    return evalGood == targetGood;
  }

  if (faninIndex >= undefFanins.size()) {
    return false;
  }

  auto &allNodes = simulator.getNodes();
  for (std::size_t idx = faninIndex; idx < undefFanins.size(); ++idx) {
    std::swap(undefFanins[faninIndex], undefFanins[idx]);
    Node *faninNode = undefFanins[faninIndex];

    std::vector<LogicValue> snapshot(allNodes.size());
    for (std::size_t i = 0; i < allNodes.size(); ++i) {
      snapshot[i] = allNodes[i].getValue(0);
    }

    for (LogicValue val : {ZERO, ONE}) {
      faninNode->setValue(val, 0);
      std::vector<Node *> tempDF, tempJF;
      if (!implyAndCheck(simulator, *faninNode, tempDF, tempJF)) {
        restoreSnapshot(allNodes, snapshot);
        continue;
      }
      if (justifyNodeFanins(simulator, jfNode, undefFanins, faninIndex + 1)) {
        return true;
      }
      restoreSnapshot(allNodes, snapshot);
    }

    std::swap(undefFanins[faninIndex], undefFanins[idx]);
  }
  return false;
}

} // namespace

bool hasDAtPrimaryOutput(Simulator &simulator) {
  for (auto po : simulator.getPrimaryOutputs()) {
    if (po->getValue(0) == D || po->getValue(0) == SD) {
      return true;
    }
  }
  return false;
}

bool finalizePrimaryInputs(Simulator &simulator) {
  auto &allNodes = simulator.getNodes();
  for (auto pi : simulator.getPrimaryInputs()) {
    if (pi->getValue(0) != UNDEF) {
      continue;
    }

    std::vector<LogicValue> snapshot(allNodes.size());
    for (std::size_t i = 0; i < allNodes.size(); ++i) {
      snapshot[i] = allNodes[i].getValue(0);
    }

    bool assigned = false;
    for (LogicValue val : {ZERO, ONE}) {
      pi->setValue(val, 0);
      std::vector<Node *> tempDF, tempJF;
      if (!implyAndCheck(simulator, *pi, tempDF, tempJF)) {
        restoreSnapshot(allNodes, snapshot);
        continue;
      }
      if (!hasDAtPrimaryOutput(simulator)) {
        restoreSnapshot(allNodes, snapshot);
        continue;
      }
      assigned = true;
      break;
    }

    if (!assigned) {
      restoreSnapshot(allNodes, snapshot);
      return false;
    }
  }

  return true;
}

int dAlgorithm_impl(Simulator &simulator, bool atpg, unsigned nodeNum_arg,
                    LogicValue faultValue_arg, unsigned DF_arg,
                    unsigned JF_arg) {
  g_dalgCallCount = 0; // reset guard for each run
  g_dalgLimitHit = false;
  g_dalgTraceCount = 0;
  simulator.simulationInit();
  simulator.circuitInit();

  unsigned nodeNum = nodeNum_arg;
  LogicValue faultValue = faultValue_arg;
  std::string outputPath = "";
  if (!atpg) {
    auto parsedArgs = parseCommandArgs(simulator.getCommandArgs());
    if (parsedArgs.size() < 3) {
      printf("DALG usage: DALG <fault_node> <fault_value> <output_file>\n");
      return 0;
    }
    nodeNum = static_cast<unsigned>(std::stoul(parsedArgs[0]));
    faultValue = parsedArgs[1] == "1" ? ONE : ZERO;
    outputPath = parsedArgs[2];
  }

  syntacticComplexityOrientedAccessibilityAnalysis_impl(simulator, "", true);

  auto &nodes = simulator.getNodes();
  auto it =
      std::find_if(nodes.begin(), nodes.end(), [nodeNum](const Node &node) {
        return node.getNum() == nodeNum;
      });

  if (it == nodes.end()) {
    printf("Fault node %d not found.\n", nodeNum);
    return 0;
  }

  auto &faultNode = *it;
  LogicValue faultSignal = (faultValue == ONE) ? LogicValue::SD : LogicValue::D;
  faultNode.setValue(faultSignal, 0);
  std::vector<Node *> nodeVec;
  nodeVec.push_back(&faultNode);
  // std::cout << "Trace D-Algorithm steps:\n";
  std::vector<Node *> extraNodes = {};
  if (!dalg(simulator, nodeVec, extraNodes, DF_arg, JF_arg)) {
    // std::cout << "No test pattern found for the given fault.\n";
    // std::ofstream outfile(outputPath, std::ios::out | std::ios::trunc);
    // if (!outfile) {
    //   std::cerr << "Error creating output file: " << outputPath << "\n";
    // }
    return 0;
  } else {
    if (!atpg) {
      std::ofstream outfile;
      outfile.open(outputPath);
      if (!outfile) {
        std::cerr << "Error opening output file: " << outputPath << "\n";
        return 0;
      }
      bool first = true;
      for (const auto x : simulator.getPrimaryInputs()) {
        if (!first)
          outfile << ',';
        first = false;
        outfile << x->getNum();
      }
      outfile << '\n';
      first = true;
      for (const auto x : simulator.getPrimaryInputs()) {
        if (!first)
          outfile << ',';
        first = false;
        outfile << ((x->getValue(0) == ONE || x->getValue(0) == D)
                        ? '1'
                        : ((x->getValue(0) == ZERO || x->getValue(0) == SD)
                               ? '0'
                               : (x->getValue(0) == HIGHZ ? 'z' : 'x')));
      }
      outfile << '\n';
      return 1;
    } else {
      // std::cout << "Test pattern found for the given fault.\n";
      return 1;
    }
  }
}

// Helper function to get CC value for a node based on target value
int getJFrontierScore(Node *node, unsigned JF_arg) {
  if (JF_arg == 0) {
    return 0; // No scoring, use insertion order
  }
  // JF_arg = 1: Select based on CC1/CC0 depending on target value
  LogicValue targetGood = toGoodLogicValue(node->getValue(0));
  if (targetGood == ONE) {
    return node->getCC1(); // Lower CC1 = easier to justify to 1
  } else if (targetGood == ZERO) {
    return node->getCC0(); // Lower CC0 = easier to justify to 0
  }
  return 999999; // UNDEF target, use high score
}

// Helper function to justify J-Frontier nodes recursively
bool justifyJFrontier(Simulator &simulator, std::vector<Node *> &JFrontier,
                      std::size_t index, unsigned JF_arg) {
  // Base case: all JF nodes processed
  if (index >= JFrontier.size()) {
    return true;
  }

  // JF_arg = 1: Sort remaining nodes by CC value (lowest first)
  if (JF_arg == 1 && index < JFrontier.size()) {
    std::sort(
        JFrontier.begin() + index, JFrontier.end(), [JF_arg](Node *a, Node *b) {
          return getJFrontierScore(a, JF_arg) < getJFrontierScore(b, JF_arg);
        });
  }

  Node *jfNode = JFrontier[index];

  const LogicValue targetGood = toGoodLogicValue(jfNode->getValue(0));
  if (targetGood == UNDEF) {
    return false;
  }

  LogicValue evalVal = evaluate(jfNode, 0);
  LogicValue evalGood = toGoodLogicValue(evalVal);

  // If this JF node is already justified (not UNDEF), ensure it matches target
  if (evalVal != UNDEF) {
    if (evalGood != targetGood) {
      return false;
    }
    return justifyJFrontier(simulator, JFrontier, index + 1, JF_arg);
  }

  // Collect UNDEF fanin nodes
  std::vector<Node *> undefFanins;
  for (unsigned i = 0; i < jfNode->getFin(); ++i) {
    Node *faninNode = jfNode->getUnodes()[i];
    if (faninNode->getValue(0) == UNDEF) {
      undefFanins.push_back(faninNode);
    }
  }

  // If no UNDEF fanins but still UNDEF output, justification failed
  if (undefFanins.empty()) {
    return false;
  }

  if (!justifyNodeFanins(simulator, jfNode, undefFanins, 0)) {
    return false;
  }

  return justifyJFrontier(simulator, JFrontier, index + 1, JF_arg);
}

// Core recursive D-Algorithm step.
//
// Each call represents one level of the search tree. The flow is:
//   1. Imply forward from the current node assignments, propagating D/D' values
//      and building the D-frontier (gates with D/D' on at least one input but
//      UNDEF output — candidates to propagate the fault toward a PO).
//   2. If a D/D' reaches a PO and the J-frontier is empty, we have a complete
//      test — assign any remaining UNDEF PIs and return true.
//   3. If a D/D' reaches a PO but the J-frontier is non-empty, we still need
//      to justify the UNDEF gates (gates with a known output but unset inputs).
//      justifyJFrontier handles this recursively.
//   4. If no D/D' at PO yet, pick a D-frontier gate (DF_arg controls the
//      heuristic — e.g. lowest/highest level, SCOAP CC), set its output to D
//      or D', and recurse. On failure, restore the snapshot and try the next
//      D-frontier choice.
//
// DF_arg: D-frontier selection heuristic (0=no-limit, 1=highest, 2=lowest,
//         3=SCOAP CC)
// JF_arg: J-frontier justification order (0=default, 1=SCOAP CC-guided)
bool dalg(Simulator &simulator, std::vector<Node *> &nodes,
          std::vector<Node *> &extraNodes, unsigned DF_arg, unsigned JF_arg) {
  if (++g_dalgCallCount > kMaxDalgCalls) {
    if (!g_dalgLimitHit) {
      // std::cout << "DALG search limit reached (" << kMaxDalgCalls << "
      // calls), aborting.\n";
      g_dalgLimitHit = true;
    }
    return false;
  }
  std::vector<Node *> DFrontier;
  std::vector<Node *> JFrontier;
  // printInfo(simulator, nodes, DFrontier, JFrontier);
  // ------------------
  // If there are updated fanin nodes from previous level, imply them first
  for (auto node : extraNodes) {
    // Set to D-frontier
    if (!implyAndCheck(simulator, *node, DFrontier, JFrontier))
      return false;
  }
  // For each D-frontier selected node
  for (auto node : nodes) {
    // Imply & check
    // Forward
    if (!implyAndCheck(simulator, *node, DFrontier, JFrontier))
      return false;
  }
  // Check if any D/D' at PO
  for (auto node : simulator.getPrimaryOutputs()) {
    if (node->getValue(0) == D || node->getValue(0) == SD) {
      if (JFrontier.empty()) {
        // std::cout << "D/D' at PO Node " << node->getNum() << "\n";

        if (!finalizePrimaryInputs(simulator)) {
          return false;
        }

        // std::cout << "Test pattern found:\n";
        // for (auto pi : simulator.getPrimaryInputs()) {
        //   std::cout << "PI Node " << pi->getNum() << " Value: " <<
        //   logicValueName(pi->getValue(0)) << "\n";
        // }
        return true;
      } else {
        // J-Frontier justification: D/D' at PO but JF not empty
        // Need to justify all JF nodes by setting their inputs appropriately
        if (g_dalgTraceCount < kMaxTracePrints) {
          // std::cout << "D/D' at PO Node " << node->getNum()
          //           << " but J-Frontier not empty (" << JFrontier.size()
          //           << "). Justifying...\n";
          ++g_dalgTraceCount;
        }

        // Take snapshot before trying justification
        std::vector<LogicValue> jfSnapshot;
        auto &allNodes = simulator.getNodes();
        jfSnapshot.reserve(allNodes.size());
        for (const auto &n : allNodes) {
          jfSnapshot.push_back(n.getValue(0));
        }

        // Try to justify JF nodes recursively
        if (justifyJFrontier(simulator, JFrontier, 0, JF_arg)) {
          if (!hasDAtPrimaryOutput(simulator)) {
            restoreSnapshot(allNodes, jfSnapshot);
            return false;
          }

          if (!finalizePrimaryInputs(simulator)) {
            restoreSnapshot(allNodes, jfSnapshot);
            return false;
          }

          // std::cout << "J-Frontier justified successfully.\n";

          // std::cout << "Test pattern found:\n";
          // for (auto pi : simulator.getPrimaryInputs()) {
          //   std::cout << "PI Node " << pi->getNum() << " Value: " <<
          //   logicValueName(pi->getValue(0)) << "\n";
          // }
          return true;
        } else {
          // Restore snapshot if justification failed and return false to
          // backtrack
          for (std::size_t j = 0; j < allNodes.size(); ++j) {
            allNodes[j].setValue(jfSnapshot[j], 0);
          }
          return false; // Backtrack to try different D-frontier choices
        }
      }
    }
  }
  // No D/D' at PO, need to select next D-frontier node(s)
  std::vector<std::vector<Node *>> choices;
  int n = static_cast<int>(DFrontier.size());
  // Limit D-frontier combinations to avoid exponential explosion
  const int maxDFrontier = 10; // 2^10 = 1024 combinations max
  int effectiveN = std::min(n, maxDFrontier);

  for (int mask = 1; mask < (1 << effectiveN); ++mask) { // skip 0 (empty)
    std::vector<Node *> combo;
    for (int i = 0; i < effectiveN; ++i) {
      if (mask & (1 << i))
        combo.push_back(DFrontier[i]);
    }
    choices.push_back(std::move(combo));
  }

  // Sort choices based on DF_arg selection strategy
  // Each choice is scored by the "best" node in that combination
  auto getChoiceScore = [DF_arg](const std::vector<Node *> &combo) -> int {
    if (combo.empty())
      return 0;
    switch (DF_arg) {
    case 0: { // Lowest node number
      int minNum = combo[0]->getNum();
      for (auto *n : combo) {
        if (static_cast<int>(n->getNum()) < minNum)
          minNum = n->getNum();
      }
      return minNum; // Lower is better, so sort ascending
    }
    case 1: { // Highest node number
      int maxNum = combo[0]->getNum();
      for (auto *n : combo) {
        if (static_cast<int>(n->getNum()) > maxNum)
          maxNum = n->getNum();
      }
      return -maxNum; // Higher is better, negate for ascending sort
    }
    case 2: { // Highest level
      int maxLvl = combo[0]->getLevel();
      for (auto *n : combo) {
        if (n->getLevel() > maxLvl)
          maxLvl = n->getLevel();
      }
      return -maxLvl; // Higher is better, negate for ascending sort
    }
    case 3: { // Lowest CO (SCOAP observability)
      int minCO = combo[0]->getCO();
      for (auto *n : combo) {
        if (n->getCO() < minCO)
          minCO = n->getCO();
      }
      return minCO; // Lower is better, so sort ascending
    }
    default:
      return 0;
    }
  };

  // Sort choices so that best choice is at the front
  std::sort(choices.begin(), choices.end(),
            [&](const std::vector<Node *> &a, const std::vector<Node *> &b) {
              return getChoiceScore(a) < getChoiceScore(b);
            });

  while (!choices.empty()) {
    // Pop from front (best choice first based on DF_arg strategy)
    auto choice = choices.front();
    choices.erase(choices.begin());
    // ------------------ Take snapshot
    std::vector<LogicValue> snapshot;
    auto &allNodes = simulator.getNodes();
    snapshot.reserve(allNodes.size());
    for (const auto &node : allNodes) {
      snapshot.push_back(node.getValue(0));
    }
    // ------------------ Try this choice
    std::vector<Node *> impliedInNodes;
    bool xorFlag = false;
    for (auto node : choice) {

      // Set inputs to non-controlling values to propagate D/D'
      for (unsigned i = 0; i < node->getFin(); ++i) {
        Node *faninNode = node->getUnodes()[i];
        if (faninNode->getValue(0) == UNDEF) {
          switch (node->getType()) {
          case AND:
          case NAND:
            faninNode->setValue(ONE, 0);
            break;
          case OR:
          case NOR:
            faninNode->setValue(ZERO, 0);
            break;
          case XOR:
          case XNOR:
            xorFlag = true;
          default:
            break;
          }
          impliedInNodes.push_back(faninNode);
        }
      }
      node->setValue(evaluate(node, 0), 0);
    }
    if (!xorFlag) {
      // Implied fanin nodes from the chosen D-frontier gate become the new
      // node set for the next recursive call. Their implications may introduce
      // additional J-frontier entries, which are built fresh each call rather
      // than accumulated globally — avoids stale JF state across backtrack levels.
      if (!dalg(simulator, choice, impliedInNodes, DF_arg, JF_arg)) {
        // Restore snapshot and try next D-frontier choice
        for (std::size_t j = 0; j < allNodes.size(); ++j) {
          allNodes[j].setValue(snapshot[j], 0);
        }
        // Continue to try next choice instead of giving up
        continue;
      } else {
        return true;
      }
    } else {
      std::vector<Node *> xorNodes;
      for (auto node : impliedInNodes) {
        if (node->getValue(0) == UNDEF) {
          xorNodes.push_back(node);
        }
      }
      // Separate snapshot for xor nodes
      std::vector<LogicValue> xorSnapshot;
      xorSnapshot.reserve(allNodes.size());
      for (const auto &node : allNodes) {
        xorSnapshot.push_back(node.getValue(0));
      }
      // Try all 2^n combinations
      const std::size_t totalCombos = static_cast<std::size_t>(1)
                                      << xorNodes.size();
      for (std::size_t mask = 0; mask < totalCombos; ++mask) {
        for (std::size_t bit = 0; bit < xorNodes.size(); ++bit) {
          xorNodes[bit]->setValue(
              (mask & (static_cast<std::size_t>(1) << bit)) ? ONE : ZERO, 0);
        }
        if (dalg(simulator, choice, impliedInNodes, DF_arg, JF_arg)) {
          return true;
        } else {
          // Restore values
          for (std::size_t j = 0; j < allNodes.size(); ++j) {
            allNodes[j].setValue(xorSnapshot[j], 0);
          }
        }
      }
      // All XOR combinations failed, restore snapshot and try next D-frontier
      // choice
      for (std::size_t j = 0; j < allNodes.size(); ++j) {
        allNodes[j].setValue(snapshot[j], 0);
      }
      // Continue to try next D-frontier choice instead of giving up
      continue;
    }
  }
  // All D-frontier choices exhausted
  return false;
}

bool implyAndCheck(Simulator &simulator, Node &node,
                   std::vector<Node *> &DFrontier,
                   std::vector<Node *> &JFrontier) {
  // If failed, return to upper level (d-alg), and let it revert and try another
  // one, no snapshot needed here.
  std::vector<Node *> stack;
  // Forward implication
  for (unsigned i = 0; i < node.getFout(); ++i) {
    Node *fanoutNode = node.getDnodes()[i];
    auto newVal = evaluate(fanoutNode, 0);
    auto fanoutVal = fanoutNode->getValue(0);
    auto fanoutValD =
        fanoutVal == D ? ONE : (fanoutVal == SD ? ZERO : fanoutVal);
    if (fanoutVal == UNDEF && newVal != UNDEF) {
      fanoutNode->setValue(newVal, 0);
      stack.push_back(fanoutNode);
    } else if (fanoutVal == UNDEF && newVal == UNDEF &&
               (node.getValue(0) == D || node.getValue(0) == SD)) {
      DFrontier.push_back(fanoutNode);
    } else if (newVal != UNDEF && fanoutVal != newVal && fanoutValD != newVal) {
      // Conflict
      return false;
    }
  }
  // Further imply and check for updated nodes
  while (!stack.empty()) {
    auto next = stack.back();
    stack.pop_back();
    if (!implyAndCheck(simulator, *next, DFrontier, JFrontier))
      return false;
  }
  // For recursive calls on foward implication nodes, backward implication will
  // not update new values. Backward implication
  auto currentVal = node.getValue(0);
  if (currentVal == D)
    currentVal = ONE;
  else if (currentVal == SD)
    currentVal = ZERO;
  for (unsigned i = 0; i < node.getFin(); ++i) {
    Node *faninNode = node.getUnodes()[i];
    if (faninNode->getValue(0) == UNDEF) {
      switch (node.getType()) {
      case IPT:
        break;
      case BRCH:
      case BUF:
        faninNode->setValue(currentVal, 0);
        stack.push_back(faninNode);
        break;
      case NOT:
        faninNode->setValue(currentVal == ONE ? ZERO : ONE, 0);
        stack.push_back(faninNode);
        break;
      case AND:
        if (currentVal == ONE) {
          faninNode->setValue(ONE, 0);
          stack.push_back(faninNode);
        }
        break;
      case NAND:
        if (currentVal == ZERO) {
          faninNode->setValue(ONE, 0);
          stack.push_back(faninNode);
        }
        break;
      case OR:
        if (currentVal == ZERO) {
          faninNode->setValue(ZERO, 0);
          stack.push_back(faninNode);
        }
        break;
      case NOR:
        if (currentVal == ONE) {
          faninNode->setValue(ZERO, 0);
          stack.push_back(faninNode);
        }
        break;
      case XNOR:
      case XOR:
        faninNode->setValue(ONE, 0);
        auto newVal = evaluate(&node, 0);
        if (newVal != currentVal) {
          faninNode->setValue(ZERO, 0);
          auto newVal = evaluate(&node, 0);
          if (newVal == currentVal) {
            stack.push_back(faninNode);
          } else {
            faninNode->setValue(UNDEF, 0);
          }
        } else {
          stack.push_back(faninNode);
        }
        break;
      }
    }
  }
  if (node.getValue(0) != UNDEF && evaluate(&node, 0) == UNDEF) {
    // Avoid duplicating the same gate in the justification frontier
    if (std::find(JFrontier.begin(), JFrontier.end(), &node) ==
        JFrontier.end()) {
      JFrontier.push_back(&node);
    }
  }
  while (!stack.empty()) {
    auto &next = *stack.back();
    stack.pop_back();
    if (!implyAndCheck(simulator, next, DFrontier, JFrontier))
      return false;
  }
  return true;
}

} // namespace logicsim
