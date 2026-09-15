#include "../includes/core/pfs.hpp"
#include "../includes/Node.hpp"
#include "../includes/Simulator.hpp"
#include "../includes/core/eds.hpp"
#include <algorithm>
#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace logicsim {

/**
 * Parallel Fault Simulation (PFS) - Optimized Implementation with C++11
 * Threading
 *
 * Key optimizations:
 * 1. Mask-based fault injection: Faults injected via f_and_mask and f_or_mask.
 *    - Stuck-at-0: f_and_mask = 0, f_or_mask = 0
 *    - Stuck-at-1: f_or_mask = 1
 *
 * 2. Single-pass levelized simulation: All bit positions evaluated in one pass.
 *    - Bit 0: Good (fault-free) circuit
 *    - Bits 1 to SystemWidth-1: Faulty circuits
 *
 * 3. Bit-parallel simulation: Up to (SystemWidth-1) faults per batch.
 *
 * 4. Level-based thread parallelism (C++11):
 *    - Nodes at the same level are independent and can be evaluated in parallel
 *    - Uses std::async for task-based parallelism within each level
 *    - Threshold-based: only parallelize levels with enough nodes to amortize
 * overhead
 */

// Number of hardware threads available
static const unsigned NUM_THREADS = std::thread::hardware_concurrency() > 0
                                        ? std::thread::hardware_concurrency()
                                        : 4;

// Minimum nodes per level to enable parallel processing (avoid thread overhead)
static const size_t PARALLEL_THRESHOLD = 8;

// Structure to hold nodes grouped by level for efficient parallel processing
struct LevelizedCircuit {
  std::vector<std::vector<Node *>> levels; // levels[i] = nodes at level i
  int maxLevel;

  void build(std::vector<Node *> &topoOrder) {
    if (topoOrder.empty()) {
      maxLevel = -1;
      return;
    }
    maxLevel = topoOrder.back()->getLevel();
    levels.resize(maxLevel + 1);
    for (auto *node : topoOrder) {
      int lvl = node->getLevel();
      if (lvl >= 0) {
        levels[lvl].push_back(node);
      }
    }
  }
};

// Helper: Reset all masks to neutral (no fault) for all bit positions
static void resetMasks(std::vector<Node> &nodes) {
  for (auto &node : nodes) {
    for (unsigned bit = 0; bit < SystemWidth; ++bit) {
      node.setFAndMask(1, bit); // neutral: pass through
      node.setFOrMask(0, bit);  // neutral: no forcing
    }
  }
}

// Helper: Inject a fault at a specific node for a specific bit position
static bool injectFault(std::vector<Node> &nodes, const Fault &fault,
                        unsigned bitPos) {
  for (auto &node : nodes) {
    if (node.getNum() == static_cast<unsigned>(fault.node_num)) {
      if (fault.fault_value == 0) {
        node.setFAndMask(0, bitPos);
        node.setFOrMask(0, bitPos);
      } else {
        node.setFAndMask(1, bitPos);
        node.setFOrMask(1, bitPos);
      }
      return true;
    }
  }
  return false;
}

// Helper: Evaluate a single node for all bit positions
static void evaluateNode(Node *nodePtr, unsigned numBits) {
  if (nodePtr->getNtype() == NodeType::PI) {
    // For PI nodes: only evaluate if there's a fault (non-neutral mask)
    for (unsigned bit = 0; bit < numBits; ++bit) {
      if (nodePtr->getFAndMask(bit) != 1 || nodePtr->getFOrMask(bit) != 0) {
        nodePtr->setValue(evaluate(nodePtr, bit), bit);
      }
    }
  } else {
    // Regular gate: evaluate all bit positions
    for (unsigned bit = 0; bit < numBits; ++bit) {
      nodePtr->setValue(evaluate(nodePtr, bit), bit);
    }
  }
}

// Helper: Process a range of nodes (for parallel worker)
static void evaluateNodeRange(Node **nodes, size_t start, size_t end,
                              unsigned numBits) {
  for (size_t i = start; i < end; ++i) {
    evaluateNode(nodes[i], numBits);
  }
}

// Helper: Parallel levelized simulation using C++11 std::async
// Nodes at the same level are evaluated in parallel
static void simulateAllBitsParallel(LevelizedCircuit &circuit,
                                    unsigned numBits) {
  for (int lvl = 0; lvl <= circuit.maxLevel; ++lvl) {
    auto &nodesAtLevel = circuit.levels[lvl];
    size_t numNodes = nodesAtLevel.size();

    if (numNodes == 0)
      continue;

    // For small levels, sequential processing avoids thread overhead
    if (numNodes < PARALLEL_THRESHOLD) {
      for (auto *nodePtr : nodesAtLevel) {
        evaluateNode(nodePtr, numBits);
      }
      continue;
    }

    // Parallel processing for larger levels
    // Divide nodes among available threads
    size_t nodesPerThread = (numNodes + NUM_THREADS - 1) / NUM_THREADS;
    std::vector<std::future<void>> futures;
    futures.reserve(NUM_THREADS);

    Node **nodeArray = nodesAtLevel.data();

    for (unsigned t = 0; t < NUM_THREADS; ++t) {
      size_t start = t * nodesPerThread;
      if (start >= numNodes)
        break;
      size_t end = std::min(start + nodesPerThread, numNodes);

      futures.push_back(std::async(std::launch::async, evaluateNodeRange,
                                   nodeArray, start, end, numBits));
    }

    // Wait for all threads at this level before proceeding to next level
    for (auto &f : futures) {
      f.get();
    }
  }
}

// Alternative: Sequential simulation (for comparison/fallback)
static void simulateAllBitsSequential(LevelizedCircuit &circuit,
                                      unsigned numBits) {
  for (int lvl = 0; lvl <= circuit.maxLevel; ++lvl) {
    for (auto *nodePtr : circuit.levels[lvl]) {
      evaluateNode(nodePtr, numBits);
    }
  }
}

std::unordered_set<Fault>
parallelFaultSim_impl(Simulator &simulator, bool atpg,
                      std::vector<std::vector<unsigned>> patternMatrix_arg,
                      std::unordered_set<Fault> faultList_arg) {

  std::unordered_set<Fault> detectedFaults;
  std::vector<std::vector<unsigned>> patternMatrix;
  std::unordered_set<Fault> faultList;
  std::string pattern_filename, fault_filename, output_filename;

  // Parse input files or use provided arguments
  if (!atpg) {
    std::stringstream ss(simulator.getCommandArgs());
    ss >> pattern_filename >> fault_filename >> output_filename;

    if (pattern_filename.empty() || fault_filename.empty() ||
        output_filename.empty()) {
      std::cerr
          << "PFS requires pattern file, fault file, and output file paths.\n";
      return {};
    }

    std::ifstream patternFile(pattern_filename);
    if (!patternFile.is_open()) {
      std::cerr << "Could not open pattern file: " << pattern_filename << "\n";
      return {};
    }

    std::string patternLine, token;
    while (std::getline(patternFile, patternLine)) {
      if (patternLine.empty())
        continue;
      std::vector<unsigned> currentLine;
      std::stringstream lineStream(patternLine);
      while (std::getline(lineStream, token, ',')) {
        currentLine.push_back(std::stoi(token));
      }
      patternMatrix.push_back(std::move(currentLine));
    }
    patternFile.close();

    std::ifstream faultFile(fault_filename);
    if (!faultFile.is_open()) {
      std::cerr << "Could not open fault file: " << fault_filename << "\n";
      return {};
    }

    std::string faultLine;
    while (std::getline(faultFile, faultLine)) {
      if (faultLine.empty())
        continue;
      std::stringstream fss(faultLine);
      std::string nodeStr;
      Fault fault;

      std::getline(fss, nodeStr, '@');
      fault.node_num = std::stoi(nodeStr);
      std::getline(fss, nodeStr);
      fault.fault_value = std::stoi(nodeStr);

      faultList.insert(fault);
    }
    faultFile.close();

    if (patternMatrix.empty() || patternMatrix[0].empty()) {
      std::cerr << "Pattern file is empty or malformed\n";
      return {};
    }
  } else {
    patternMatrix = std::move(patternMatrix_arg);
    faultList = std::move(faultList_arg);
  }

  simulator.circuitInit();

  // Compute level information
  simulator.levelize();

  // Build topological order and levelized structure
  std::vector<Node *> topoOrder;
  topoOrder.reserve(simulator.getNumNodes());
  for (auto &n : simulator.getNodes()) {
    topoOrder.push_back(&n);
  }
  std::sort(topoOrder.begin(), topoOrder.end(),
            [](const Node *a, const Node *b) {
              return a->getLevel() < b->getLevel();
            });

  // Build levelized circuit structure for parallel processing
  LevelizedCircuit circuit;
  circuit.build(topoOrder);

  // Convert fault set to vector for indexed access
  std::vector<Fault> faultVec(faultList.begin(), faultList.end());

  const unsigned maxFaultsPerBatch = SystemWidth - 1;

  // Debug flag
  bool doDebug = false;
  const char *dbgenv = std::getenv("PFS_DEBUG");
  if (dbgenv && std::string(dbgenv) == "1")
    doDebug = true;

  // Check if parallel mode is enabled (can be disabled for debugging)
  bool useParallel = true;
  const char *parenv = std::getenv("PFS_SEQUENTIAL");
  if (parenv && std::string(parenv) == "1")
    useParallel = false;

  // Process each test pattern
  for (size_t patIdx = 1; patIdx < patternMatrix.size(); patIdx++) {

    // Process faults in batches
    size_t faultIdx = 0;
    while (faultIdx < faultVec.size()) {
      unsigned batchSize = std::min(static_cast<size_t>(maxFaultsPerBatch),
                                    faultVec.size() - faultIdx);
      unsigned numBits = batchSize + 1;

      // Step 1: Reset all node values and masks
      for (auto &node : simulator.getNodes()) {
        for (unsigned bit = 0; bit < numBits; ++bit) {
          node.setValue(LogicValue::UNDEF, bit);
        }
      }
      resetMasks(simulator.getNodes());

      // Step 2: Apply primary input values to all bit positions
      for (size_t inIdx = 0; inIdx < patternMatrix[0].size() &&
                             inIdx < static_cast<size_t>(simulator.getNPI());
           ++inIdx) {
        unsigned nodeID = patternMatrix[0][inIdx];
        int value = patternMatrix[patIdx][inIdx];
        LogicValue lv = (value == 1) ? LogicValue::ONE : LogicValue::ZERO;

        for (auto &node : simulator.getNodes()) {
          if (node.getNum() == nodeID) {
            for (unsigned bit = 0; bit < numBits; ++bit) {
              node.setValue(lv, bit);
            }
            break;
          }
        }
      }

      // Step 3: Inject faults via masks
      for (unsigned f = 0; f < batchSize; ++f) {
        const Fault &fault = faultVec[faultIdx + f];
        unsigned bitPos = f + 1;
        injectFault(simulator.getNodes(), fault, bitPos);
      }

      // Step 4: Levelized simulation (parallel or sequential)
      if (useParallel) {
        simulateAllBitsParallel(circuit, numBits);
      } else {
        simulateAllBitsSequential(circuit, numBits);
      }

      // Step 5: Fault detection
      if (doDebug) {
        std::cerr << "PFS-debug: Pattern " << patIdx
                  << ", batch starting at fault " << faultIdx << "\n";
        std::cerr << "PFS-debug: good PO values: ";
        for (int i = 0; i < simulator.getNPO(); ++i) {
          auto v = simulator.getPrimaryOutputs()[i]->getValue(0);
          std::cerr << (v == LogicValue::ONE
                            ? '1'
                            : (v == LogicValue::ZERO ? '0' : 'x'));
        }
        std::cerr << "\n";
      }

      for (unsigned f = 0; f < batchSize; ++f) {
        const Fault &fault = faultVec[faultIdx + f];
        unsigned bitPos = f + 1;
        bool detected = false;

        for (int i = 0; i < simulator.getNPO(); ++i) {
          LogicValue goodVal = simulator.getPrimaryOutputs()[i]->getValue(0);
          LogicValue faultyVal =
              simulator.getPrimaryOutputs()[i]->getValue(bitPos);

          if (goodVal != faultyVal) {
            detectedFaults.insert(fault);
            detected = true;
            break;
          }
        }

        if (doDebug) {
          std::cerr << "PFS-debug: fault " << fault.node_num << "@"
                    << fault.fault_value << " -> ";
          for (int i = 0; i < simulator.getNPO(); ++i) {
            auto v = simulator.getPrimaryOutputs()[i]->getValue(bitPos);
            std::cerr << (v == LogicValue::ONE
                              ? '1'
                              : (v == LogicValue::ZERO ? '0' : 'x'));
          }
          std::cerr << " (detected=" << detected << ")\n";
        }
      }

      faultIdx += batchSize;
    }
  }

  // Write output file
  if (!atpg) {
    size_t last_slash = output_filename.find_last_of("/\\");
    if (last_slash != std::string::npos) {
      std::string dir = output_filename.substr(0, last_slash);
      std::string cmd = "mkdir -p '" + dir + "'";
      system(cmd.c_str());
    }

    std::ofstream outStream(output_filename);
    if (!outStream.is_open()) {
      std::cerr << "Could not open output file: " << output_filename << "\n";
      return {};
    }

    for (const auto &fault : detectedFaults) {
      outStream << fault.node_num << "@" << fault.fault_value << "\n";
    }
    outStream.close();
  }

  return detectedFaults;
}

} // namespace logicsim
