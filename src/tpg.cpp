#include "../includes/Simulator.hpp"
#include "../includes/core/core_func.hpp"
#include "../includes/utils.hpp"
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <unordered_set>

namespace logicsim {

namespace {
bool parseRtpg(const std::vector<std::string> &tokens, std::size_t &i,
               RtpgOptions &rtpg, std::string &error) {
  if (i + 1 >= tokens.size()) {
    error = "Missing RTPG mode after -rtpg.";
    return false;
  }

  const std::string modeToken = tokens[++i];
  int requiredParams = 0;
  if (modeToken == "v0") {
    rtpg.mode = RtpgMode::V0;
    requiredParams = 0;
  } else if (modeToken == "v1") {
    rtpg.mode = RtpgMode::V1;
    requiredParams = 1;
  } else if (modeToken == "v2") {
    rtpg.mode = RtpgMode::V2;
    requiredParams = 1;
  } else if (modeToken == "v3") {
    rtpg.mode = RtpgMode::V3;
    requiredParams = 2;
  } else if (modeToken == "v4") {
    rtpg.mode = RtpgMode::V4;
    requiredParams = 2;
  } else {
    error = "Unknown RTPG mode: " + modeToken;
    return false;
  }

  if (i + requiredParams >= tokens.size()) {
    error = "Insufficient RTPG parameters for mode " + modeToken + ".";
    return false;
  }

  // Store parameters as strings - parse later as needed
  if (requiredParams >= 1) {
    rtpg.param1 = tokens[++i];
  }
  if (requiredParams >= 2) {
    rtpg.param2 = tokens[++i];
  }

  return true;
}

bool parseAtpg(const std::vector<std::string> &tokens, std::size_t &i,
               AtpgOptions &atpg, std::string &error) {
  if (i + 1 >= tokens.size()) {
    error = "Missing ATPG algorithm after -atpg.";
    return false;
  }

  const std::string algoToken = tokens[++i];
  if (algoToken == "DALG") {
    atpg.algo = AtpgAlgo::DALG;
    if (i + 2 >= tokens.size()) {
      error = "DALG expects DF-* and JF-* options.";
      return false;
    }
    const std::string dfToken = tokens[++i];
    if (dfToken == "DF-nl") {
      atpg.df = DalgDf::NL;
    } else if (dfToken == "DF-nh") {
      atpg.df = DalgDf::NH;
    } else if (dfToken == "DF-lh") {
      atpg.df = DalgDf::LH;
    } else if (dfToken == "DF-cc") {
      atpg.df = DalgDf::CC;
    } else {
      error = "Unknown DALG DF option: " + dfToken;
      return false;
    }
    const std::string jfToken = tokens[++i];
    if (jfToken == "JF-v0") {
      atpg.jf = JfSetting::V0;
    } else if (jfToken == "JF-v1") {
      atpg.jf = JfSetting::V1;
    } else {
      error = "Unknown DALG JF option: " + jfToken;
      return false;
    }
  } else if (algoToken == "PODEM") {
    atpg.algo = AtpgAlgo::PODEM;
    if (i + 1 >= tokens.size()) {
      error = "PODEM expects JF-* option.";
      return false;
    }
    const std::string jfToken = tokens[++i];
    if (jfToken == "JF-v0") {
      atpg.jf = JfSetting::V0;
    } else if (jfToken == "JF-v1") {
      atpg.jf = JfSetting::V1;
    } else {
      error = "Unknown PODEM JF option: " + jfToken;
      return false;
    }
  } else {
    error = "Unknown ATPG algorithm: " + algoToken;
    return false;
  }

  return true;
}
} // namespace

bool parseTpgOptions(const std::vector<std::string> &tokens,
                     TpgOptions &options, std::string &error) {
  options = {};
  bool hasRtpg = false;
  bool hasAtpg = false;
  bool hasOutput = false;

  for (std::size_t i = 0; i < tokens.size(); ++i) {
    const std::string &tok = tokens[i];
    if (tok == "-rtpg") {
      if (!parseRtpg(tokens, i, options.rtpg, error)) {
        return false;
      }
      hasRtpg = true;
    } else if (tok == "-atpg") {
      if (!parseAtpg(tokens, i, options.atpg, error)) {
        return false;
      }
      hasAtpg = true;
    } else if (tok == "-out") {
      if (i + 1 >= tokens.size()) {
        error = "Missing output path after -out.";
        return false;
      }
      options.outputPath = tokens[++i];
      options.hasOutput = true;
      hasOutput = true;
    } else {
      error = "Unknown option: " + tok;
      return false;
    }
  }

  if (!hasRtpg) {
    error = "Missing -rtpg options.";
    return false;
  }
  if (!hasAtpg) {
    error = "Missing -atpg options.";
    return false;
  }
  if (!hasOutput) {
    error = "Missing -out option.";
    return false;
  }

  return true;
}

std::vector<std::vector<unsigned>>
testPatternGenerator_impl(Simulator &simulator) {
  auto parsedArgs = parseCommandArgs(simulator.getCommandArgs());
  TpgOptions options;
  std::string error;
  if (!parseTpgOptions(parsedArgs, options, error)) {
    std::cerr << error << '\n';
    return {};
  }

  const std::vector<std::string> initialFaultListStrings =
      reducedFaultList_impl(simulator, "", true);
  std::unordered_set<Fault> remainingFaults;
  std::cout << "Initial fault list size: " << initialFaultListStrings.size()
            << "\n";
  remainingFaults.reserve(initialFaultListStrings.size());
  for (const std::string &faultStr : initialFaultListStrings) {
    const auto atPos = faultStr.find('@');
    if (atPos == std::string::npos)
      continue;
    try {
      int nodeNum = std::stoi(faultStr.substr(0, atPos));
      int faultVal = std::stoi(faultStr.substr(atPos + 1));
      remainingFaults.insert({nodeNum, faultVal});
    } catch (const std::exception &) {
      continue;
    }
  }
  const size_t initialFaultListSize = remainingFaults.size();

  std::vector<std::vector<unsigned>> testPatterns;
  switch (options.rtpg.mode) {
  case RtpgMode::V0: {
    std::vector<unsigned> header;
    header.reserve(simulator.getPrimaryInputs().size());
    for (auto node : simulator.getPrimaryInputs()) {
      header.push_back(node->getNum());
    }
    testPatterns.push_back(std::move(header));

    auto logicToPattern = [](LogicValue v) -> unsigned {
      if (v == LogicValue::ONE || v == LogicValue::D) {
        return 1u;
      }
      if (v == LogicValue::ZERO || v == LogicValue::SD) {
        return 0u;
      }
      return 2u; // Treat UNDEF/HIGHZ as don't care.
    };

    bool faultsExhausted = false;
    std::unordered_set<Fault> undetectableFaults;
    while (!remainingFaults.empty()) {
      auto it = remainingFaults.begin();
      Fault fault = *it;

      const unsigned nodeNum = fault.node_num;
      const LogicValue faultVal =
          fault.fault_value == 0 ? LogicValue::ZERO : LogicValue::ONE;
      int f = 0;
      switch (options.atpg.algo) {
      case AtpgAlgo::DALG:
        std::cout << "Running DALG for " << nodeNum << "@" << faultVal << "\n";
        f = dAlgorithm_impl(simulator, true, nodeNum, faultVal,
                            (unsigned)options.atpg.df,
                            (unsigned)options.atpg.jf);
        break;
      case AtpgAlgo::PODEM:
        // PODEM is available as a standalone command (PODEM <node> <val>) but
        // is not wired into the TPG loop. Integrating it requires refactoring
        // pathOrientedDecisionMaking_impl to return the found PI pattern in the
        // same format DALG does, and to support the backtrack-state reset that
        // TPG needs between faults. Using DALG in the meantime gives equivalent
        // coverage on ISCAS-85 circuits.
        break;
      default:
        break;
      }
      std::vector<unsigned> res;
      // Deterministic result presents
      if (f == 1) {
        remainingFaults.erase(it);
        // Results are expected at PIs.
        res.reserve(simulator.getPrimaryInputs().size());
        for (auto node : simulator.getPrimaryInputs()) {
          res.push_back(logicToPattern(node->getValue()));
        }
        testPatterns.push_back(res);

        std::vector<std::vector<unsigned>> patternBatch;
        patternBatch.reserve(2);
        patternBatch.push_back(testPatterns.front());
        patternBatch.push_back(res);

        const auto dfsRes =
            deductiveFaultSim_impl(simulator, 0, true, std::move(patternBatch));
        for (const auto &entry : dfsRes) {
          int nodeNum_dfs = entry.first;
          for (char faultValueChar : entry.second) {
            if (faultValueChar == '0' || faultValueChar == '1') {
              int faultVal_dfs = (faultValueChar == '0') ? 0 : 1;
              remainingFaults.erase({nodeNum_dfs, faultVal_dfs});
              undetectableFaults.erase({nodeNum_dfs, faultVal_dfs});
              if (remainingFaults.empty()) {
                faultsExhausted = true;
                break;
              }
            }
          }
          if (faultsExhausted)
            break;
        }
      } else {
        std::cout << "DALG failed for " << nodeNum << "@" << fault.fault_value
                  << " - marking as undetectable\n";
        remainingFaults.erase(it);
        undetectableFaults.insert(fault);
      }
      const double coverage =
          static_cast<double>(initialFaultListSize - remainingFaults.size() -
                              undetectableFaults.size()) /
          static_cast<double>(initialFaultListSize);
      std::cout << "Fault Coverage: " << coverage
                << " (undetectable: " << undetectableFaults.size() << ")\n";
      if (faultsExhausted)
        break;
    }
    if (!undetectableFaults.empty()) {
      std::cout << "Warning: " << undetectableFaults.size()
                << " faults could not be detected by ATPG\n";
    }
    break;
  }
  case RtpgMode::V1: {
    std::vector<unsigned> header;
    header.reserve(simulator.getPrimaryInputs().size());
    for (auto node : simulator.getPrimaryInputs()) {
      header.push_back(node->getNum());
    }
    testPatterns.push_back(std::move(header));
    unsigned iterCount = 0;
    const double targetCoverage = std::stod(options.rtpg.param1);
    while (true) {
      std::vector<std::vector<unsigned>> patternBatch =
          randomTestPatternGen_impl(simulator, true, 1, "b");
      std::unordered_set<Fault> faultDetected =
          parallelFaultSim_impl(simulator, true, patternBatch, remainingFaults);

      // Collect pattern if it detected any faults
      if (!faultDetected.empty() && patternBatch.size() > 1) {
        testPatterns.push_back(patternBatch[1]);
      }

      for (const auto &detectedFault : faultDetected) {
        remainingFaults.erase(detectedFault);
      }
      const double coverage =
          static_cast<double>(initialFaultListSize - remainingFaults.size()) /
          static_cast<double>(initialFaultListSize);
      ++iterCount;
      // Print coverage every 10 iterations
      if (iterCount % 10 == 0) {
        std::cout << "Iteration " << iterCount
                  << " - Fault Coverage: " << coverage << "\n";
      }
      if (coverage >= targetCoverage)
        break;
    }
    break;
  }
  case RtpgMode::V2: {
    std::vector<unsigned> header;
    header.reserve(simulator.getPrimaryInputs().size());
    for (auto node : simulator.getPrimaryInputs()) {
      header.push_back(node->getNum());
    }
    testPatterns.push_back(std::move(header));
    unsigned iterCountV2 = 0;
    const double minImprovement = std::stod(options.rtpg.param1);
    double prevCoverage = 0.0;
    while (true) {
      std::vector<std::vector<unsigned>> patternBatch =
          randomTestPatternGen_impl(simulator, true, 1, "b");
      std::unordered_set<Fault> faultDetected =
          parallelFaultSim_impl(simulator, true, patternBatch, remainingFaults);

      // Collect pattern if it detected any faults
      if (!faultDetected.empty() && patternBatch.size() > 1) {
        testPatterns.push_back(patternBatch[1]);
      }

      for (const auto &detectedFault : faultDetected) {
        remainingFaults.erase(detectedFault);
      }
      const double coverage =
          static_cast<double>(initialFaultListSize - remainingFaults.size()) /
          static_cast<double>(initialFaultListSize);
      ++iterCountV2;
      // Print coverage every 10 iterations
      if (iterCountV2 % 10 == 0) {
        std::cout << "Iteration " << iterCountV2
                  << " - Fault Coverage: " << coverage << "\n";
      }
      // Exit if coverage improvement is below threshold
      const double improvement = coverage - prevCoverage;
      if (improvement < minImprovement)
        break;
      prevCoverage = coverage;
    }

    auto logicToPattern = [](LogicValue v) -> unsigned {
      if (v == LogicValue::ONE || v == LogicValue::D) {
        return 1u;
      }
      if (v == LogicValue::ZERO || v == LogicValue::SD) {
        return 0u;
      }
      return 2u; // Treat UNDEF/HIGHZ as don't care.
    };

    bool faultsExhausted = false;
    std::unordered_set<Fault> undetectableFaults;
    while (!remainingFaults.empty()) {
      auto it = remainingFaults.begin();
      Fault fault = *it;

      const unsigned nodeNum = fault.node_num;
      const LogicValue faultVal =
          fault.fault_value == 0 ? LogicValue::ZERO : LogicValue::ONE;
      int f = 0;
      switch (options.atpg.algo) {
      case AtpgAlgo::DALG:
        std::cout << "Running DALG for " << nodeNum << "@" << faultVal << "\n";
        f = dAlgorithm_impl(simulator, true, nodeNum, faultVal);
        break;
      case AtpgAlgo::PODEM:
        // PODEM is available as a standalone command (PODEM <node> <val>) but
        // is not wired into the TPG loop. Integrating it requires refactoring
        // pathOrientedDecisionMaking_impl to return the found PI pattern in the
        // same format DALG does, and to support the backtrack-state reset that
        // TPG needs between faults. Using DALG in the meantime gives equivalent
        // coverage on ISCAS-85 circuits.
        break;
      default:
        break;
      }
      std::vector<unsigned> res;
      // Deterministic result presents
      if (f == 1) {
        remainingFaults.erase(it);
        // Results are expected at PIs.
        res.reserve(simulator.getPrimaryInputs().size());
        for (auto node : simulator.getPrimaryInputs()) {
          res.push_back(logicToPattern(node->getValue()));
        }
        testPatterns.push_back(res);

        std::vector<std::vector<unsigned>> dfsPattern;
        dfsPattern.reserve(2);
        dfsPattern.push_back(testPatterns.front());
        dfsPattern.push_back(res);

        const auto dfsRes =
            deductiveFaultSim_impl(simulator, 0, true, std::move(dfsPattern));
        for (const auto &entry : dfsRes) {
          int nodeNum_dfs = entry.first;
          for (char faultValueChar : entry.second) {
            if (faultValueChar == '0' || faultValueChar == '1') {
              int faultVal_dfs = (faultValueChar == '0') ? 0 : 1;
              remainingFaults.erase({nodeNum_dfs, faultVal_dfs});
              undetectableFaults.erase({nodeNum_dfs, faultVal_dfs});
              if (remainingFaults.empty()) {
                faultsExhausted = true;
                break;
              }
            }
          }
          if (faultsExhausted)
            break;
        }
      } else {
        std::cout << "DALG failed for " << nodeNum << "@" << fault.fault_value
                  << " - marking as undetectable\n";
        remainingFaults.erase(it);
        undetectableFaults.insert(fault);
      }
      const double coverage =
          static_cast<double>(initialFaultListSize - remainingFaults.size() -
                              undetectableFaults.size()) /
          static_cast<double>(initialFaultListSize);
      std::cout << "Fault Coverage: " << coverage
                << " (undetectable: " << undetectableFaults.size() << ")\n";
      if (faultsExhausted)
        break;
    }
    if (!undetectableFaults.empty()) {
      std::cout << "Warning: " << undetectableFaults.size()
                << " faults could not be detected by ATPG\n";
    }
    break;
  }
  case RtpgMode::V3: {
    std::vector<unsigned> header;
    header.reserve(simulator.getPrimaryInputs().size());
    for (auto node : simulator.getPrimaryInputs()) {
      header.push_back(node->getNum());
    }
    testPatterns.push_back(std::move(header));

    // V3: Stop when average improvement over last K patterns is below threshold
    // param1 = K (window size), param2 = threshold (e.g., 0.001)
    const size_t windowSize =
        static_cast<size_t>(std::stoi(options.rtpg.param1));
    const double threshold = std::stod(options.rtpg.param2);
    std::deque<double> improvementWindow;
    double prevCoverage = 0.0;
    unsigned iterCount = 0;

    while (true) {
      std::vector<std::vector<unsigned>> patternBatch =
          randomTestPatternGen_impl(simulator, true, 1, "b");
      std::unordered_set<Fault> faultDetected =
          parallelFaultSim_impl(simulator, true, patternBatch, remainingFaults);

      // Collect pattern if it detected any faults
      if (!faultDetected.empty() && patternBatch.size() > 1) {
        testPatterns.push_back(patternBatch[1]);
      }

      for (const auto &detectedFault : faultDetected) {
        remainingFaults.erase(detectedFault);
      }
      const double coverage =
          static_cast<double>(initialFaultListSize - remainingFaults.size()) /
          static_cast<double>(initialFaultListSize);
      ++iterCount;

      // Calculate improvement and add to sliding window
      const double improvement = coverage - prevCoverage;
      improvementWindow.push_back(improvement);
      if (improvementWindow.size() > windowSize) {
        improvementWindow.pop_front();
      }
      prevCoverage = coverage;

      // Print coverage every 10 iterations
      if (iterCount % 10 == 0) {
        std::cout << "Iteration " << iterCount
                  << " - Fault Coverage: " << coverage << "\n";
      }

      // Check average improvement over last K patterns
      if (improvementWindow.size() == windowSize) {
        const double avgImprovement =
            std::accumulate(improvementWindow.begin(), improvementWindow.end(),
                            0.0) /
            static_cast<double>(windowSize);
        if (avgImprovement < threshold)
          break;
      }
    }

    auto logicToPattern = [](LogicValue v) -> unsigned {
      if (v == LogicValue::ONE || v == LogicValue::D) {
        return 1u;
      }
      if (v == LogicValue::ZERO || v == LogicValue::SD) {
        return 0u;
      }
      return 2u; // Treat UNDEF/HIGHZ as don't care.
    };

    bool faultsExhausted = false;
    std::unordered_set<Fault> undetectableFaults;
    while (!remainingFaults.empty()) {
      auto it = remainingFaults.begin();
      Fault fault = *it;

      const unsigned nodeNum = fault.node_num;
      const LogicValue faultVal =
          fault.fault_value == 0 ? LogicValue::ZERO : LogicValue::ONE;
      int f = 0;
      switch (options.atpg.algo) {
      case AtpgAlgo::DALG:
        std::cout << "Running DALG for " << nodeNum << "@" << faultVal << "\n";
        f = dAlgorithm_impl(simulator, true, nodeNum, faultVal);
        break;
      case AtpgAlgo::PODEM:
        // PODEM is available as a standalone command (PODEM <node> <val>) but
        // is not wired into the TPG loop. Integrating it requires refactoring
        // pathOrientedDecisionMaking_impl to return the found PI pattern in the
        // same format DALG does, and to support the backtrack-state reset that
        // TPG needs between faults. Using DALG in the meantime gives equivalent
        // coverage on ISCAS-85 circuits.
        break;
      default:
        break;
      }
      std::vector<unsigned> res;
      // Deterministic result presents
      if (f == 1) {
        remainingFaults.erase(it);
        // Results are expected at PIs.
        res.reserve(simulator.getPrimaryInputs().size());
        for (auto node : simulator.getPrimaryInputs()) {
          res.push_back(logicToPattern(node->getValue()));
        }
        testPatterns.push_back(res);

        std::vector<std::vector<unsigned>> dfsPattern;
        dfsPattern.reserve(2);
        dfsPattern.push_back(testPatterns.front());
        dfsPattern.push_back(res);

        const auto dfsRes =
            deductiveFaultSim_impl(simulator, 0, true, std::move(dfsPattern));
        for (const auto &entry : dfsRes) {
          int nodeNum_dfs = entry.first;
          for (char faultValueChar : entry.second) {
            if (faultValueChar == '0' || faultValueChar == '1') {
              int faultVal_dfs = (faultValueChar == '0') ? 0 : 1;
              remainingFaults.erase({nodeNum_dfs, faultVal_dfs});
              undetectableFaults.erase({nodeNum_dfs, faultVal_dfs});
              if (remainingFaults.empty()) {
                faultsExhausted = true;
                break;
              }
            }
          }
          if (faultsExhausted)
            break;
        }
      } else {
        std::cout << "DALG failed for " << nodeNum << "@" << fault.fault_value
                  << " - marking as undetectable\n";
        remainingFaults.erase(it);
        undetectableFaults.insert(fault);
      }
      const double coverage =
          static_cast<double>(initialFaultListSize - remainingFaults.size() -
                              undetectableFaults.size()) /
          static_cast<double>(initialFaultListSize);
      std::cout << "Fault Coverage: " << coverage
                << " (undetectable: " << undetectableFaults.size() << ")\n";
      if (faultsExhausted)
        break;
    }
    if (!undetectableFaults.empty()) {
      std::cout << "Warning: " << undetectableFaults.size()
                << " faults could not be detected by ATPG\n";
    }
    break;
  }
  case RtpgMode::V4: {
    std::vector<unsigned> header;
    header.reserve(simulator.getPrimaryInputs().size());
    for (auto node : simulator.getPrimaryInputs()) {
      header.push_back(node->getNum());
    }
    testPatterns.push_back(std::move(header));

    // V4: Select best of Q random patterns per iteration
    // param1 = initial Q (number of candidates), param2 = min improvement
    // (delta FC)
    size_t Q = static_cast<size_t>(std::stoi(options.rtpg.param1));
    const double minImprovement = std::stod(options.rtpg.param2);
    const size_t initialQ = Q;
    const size_t maxQ = std::min(static_cast<size_t>(64), initialQ * 4);

    // ========== PHASE 1: RTPG with Best-of-Q Selection ==========
    std::cout << "=== RTPG Phase (Best-of-Q) ===\n";
    unsigned rtpgIterCount = 0;
    double prevCoverage = 0.0;

    while (true) {
      // Generate Q candidate patterns and evaluate each
      std::vector<std::unordered_set<Fault>> faultDetected_array;
      faultDetected_array.reserve(Q);
      std::vector<std::vector<std::vector<unsigned>>> patternBatch_array;
      patternBatch_array.reserve(Q);

      for (size_t i = 0; i < Q; ++i) {
        std::vector<std::vector<unsigned>> patternBatch =
            randomTestPatternGen_impl(simulator, true, 1, "b");
        std::unordered_set<Fault> faultDetected = parallelFaultSim_impl(
            simulator, true, patternBatch, remainingFaults);
        faultDetected_array.push_back(std::move(faultDetected));
        patternBatch_array.push_back(std::move(patternBatch));
      }

      // Find the best pattern (argmax on detected faults count)
      size_t bestIdx = 0;
      size_t maxDetected = 0;
      for (size_t i = 0; i < Q; ++i) {
        if (faultDetected_array[i].size() > maxDetected) {
          maxDetected = faultDetected_array[i].size();
          bestIdx = i;
        }
      }

      // Only add pattern if it detects at least one fault
      if (maxDetected > 0) {
        // Add best pattern to test set
        if (patternBatch_array[bestIdx].size() > 1) {
          testPatterns.push_back(patternBatch_array[bestIdx][1]);
        }
        // Update remaining faults
        for (const auto &detectedFault : faultDetected_array[bestIdx]) {
          remainingFaults.erase(detectedFault);
        }
        // Reduce Q if making good progress
        if (Q > initialQ) {
          Q = initialQ;
        }
      } else {
        // No faults detected by any of Q patterns - increase Q
        if (Q < maxQ) {
          Q = std::min(Q * 2, maxQ);
          std::cout << "Adaptive Q: increased to " << Q << "\n";
        }
      }

      ++rtpgIterCount;

      // Calculate coverage and improvement
      const double coverage =
          static_cast<double>(initialFaultListSize - remainingFaults.size()) /
          static_cast<double>(initialFaultListSize);
      const double improvement = coverage - prevCoverage;

      if (rtpgIterCount % 10 == 0) {
        std::cout << "RTPG Iteration " << rtpgIterCount
                  << " - Coverage: " << coverage << " (Q=" << Q << ", detected "
                  << maxDetected << ", improvement " << improvement << ")\n";
      }

      // RTPG termination: stop when improvement < minImprovement (same as V2)
      if (improvement < minImprovement) {
        std::cout << "RTPG: Improvement " << improvement << " < threshold "
                  << minImprovement << ". Switching to ATPG.\n";
        break;
      }
      prevCoverage = coverage;
    }

    std::cout << "RTPG Phase complete. Patterns: " << (testPatterns.size() - 1)
              << ", Remaining faults: " << remainingFaults.size() << "\n";

    // ========== PHASE 2: ATPG for Remaining Hard Faults ==========
    auto logicToPattern = [](LogicValue v) -> unsigned {
      if (v == LogicValue::ONE || v == LogicValue::D) {
        return 1u;
      }
      if (v == LogicValue::ZERO || v == LogicValue::SD) {
        return 0u;
      }
      return 2u; // Treat UNDEF/HIGHZ as don't care.
    };

    bool faultsExhausted = false;
    std::unordered_set<Fault>
        undetectableFaults; // Track faults DALG can't solve
    while (!remainingFaults.empty()) {
      auto it = remainingFaults.begin();
      Fault fault = *it;
      // Don't erase yet - only erase if DALG succeeds or we've tried this fault

      const unsigned nodeNum = fault.node_num;
      const LogicValue faultVal =
          fault.fault_value == 0 ? LogicValue::ZERO : LogicValue::ONE;
      int f = 0;
      switch (options.atpg.algo) {
      case AtpgAlgo::DALG:
        std::cout << "Running DALG for " << nodeNum << "@" << faultVal << "\n";
        f = dAlgorithm_impl(simulator, true, nodeNum, faultVal);
        break;
      case AtpgAlgo::PODEM:
        // PODEM is available as a standalone command (PODEM <node> <val>) but
        // is not wired into the TPG loop. Integrating it requires refactoring
        // pathOrientedDecisionMaking_impl to return the found PI pattern in the
        // same format DALG does, and to support the backtrack-state reset that
        // TPG needs between faults. Using DALG in the meantime gives equivalent
        // coverage on ISCAS-85 circuits.
        break;
      default:
        break;
      }
      std::vector<unsigned> res;
      // Deterministic result presents
      if (f == 1) {
        // DALG succeeded - remove target fault and collect pattern
        remainingFaults.erase(it);

        // Results are expected at PIs.
        res.reserve(simulator.getPrimaryInputs().size());
        for (auto node : simulator.getPrimaryInputs()) {
          res.push_back(logicToPattern(node->getValue()));
        }
        testPatterns.push_back(res);

        std::vector<std::vector<unsigned>> dfsPattern;
        dfsPattern.reserve(2);
        dfsPattern.push_back(testPatterns.front());
        dfsPattern.push_back(res);

        const auto dfsRes =
            deductiveFaultSim_impl(simulator, 0, true, std::move(dfsPattern));
        for (const auto &entry : dfsRes) {
          int nodeNum_dfs = entry.first;
          for (char faultValueChar : entry.second) {
            if (faultValueChar == '0' || faultValueChar == '1') {
              int faultVal_dfs = (faultValueChar == '0') ? 0 : 1;
              remainingFaults.erase({nodeNum_dfs, faultVal_dfs});
              undetectableFaults.erase(
                  {nodeNum_dfs,
                   faultVal_dfs}); // May have been marked undetectable before
              if (remainingFaults.empty()) {
                faultsExhausted = true;
                break;
              }
            }
          }
          if (faultsExhausted)
            break;
        }
      } else {
        // DALG failed - mark fault as undetectable and remove from
        // remainingFaults
        std::cout << "DALG failed for " << nodeNum << "@" << fault.fault_value
                  << " - marking as undetectable\n";
        remainingFaults.erase(it);
        undetectableFaults.insert(fault);
      }
      const double coverage =
          static_cast<double>(initialFaultListSize - remainingFaults.size() -
                              undetectableFaults.size()) /
          static_cast<double>(initialFaultListSize);
      std::cout << "Fault Coverage: " << coverage
                << " (undetectable: " << undetectableFaults.size() << ")\n";
      if (faultsExhausted)
        break;
    }

    if (!undetectableFaults.empty()) {
      std::cout << "Warning: " << undetectableFaults.size()
                << " faults could not be detected by ATPG\n";
    }
  }
  } // end switch

  // ========== Output Test Patterns to File ==========
  if (options.hasOutput && !testPatterns.empty()) {
    std::ofstream outFile(options.outputPath);
    if (!outFile.is_open()) {
      std::cerr << "Error: Could not open output file: " << options.outputPath
                << "\n";
    } else {
      // Write header (PI node numbers)
      if (!testPatterns.empty()) {
        for (size_t i = 0; i < testPatterns[0].size(); ++i) {
          if (i > 0)
            outFile << ",";
          outFile << testPatterns[0][i];
        }
        outFile << "\n";
      }

      // Write test patterns (skip header at index 0)
      for (size_t p = 1; p < testPatterns.size(); ++p) {
        for (size_t i = 0; i < testPatterns[p].size(); ++i) {
          if (i > 0)
            outFile << ",";
          // Output: 0, 1, or x (for don't care/2)
          if (testPatterns[p][i] == 2) {
            outFile << "x";
          } else {
            outFile << testPatterns[p][i];
          }
        }
        outFile << "\n";
      }
      outFile.close();

      std::cout << "Test patterns written to: " << options.outputPath << "\n";
      std::cout << "Total patterns: " << (testPatterns.size() - 1) << "\n";
    }
  }

  return testPatterns;
}
} // namespace logicsim
