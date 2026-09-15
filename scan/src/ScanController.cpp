#include "../include/ScanController.hpp"
#include <fstream>
#include <unordered_set>

namespace scan {

static uint64_t lfsrNext(uint64_t &state) {
    uint64_t bit = __builtin_parityll(state & 0xB4BCD35CULL);
    state = ((state >> 1) | (bit << 31)) & 0xFFFFFFFFULL;
    if (state == 0) state = 1;
    return state;
}

ScanController::ScanController(Circuit &circuit, ScanChain &chain, const ScanConfig &cfg)
    : circuit_(circuit), chain_(chain), cfg_(cfg) {}

bool ScanController::detects(const std::vector<LogicVal> &piVals,
                              const std::vector<LogicVal> &ffStates,
                              const std::vector<LogicVal> &freeOutputs,
                              const std::vector<LogicVal> &freeNextFF,
                              const Fault &f) {
    circuit_.simulateWithFault(piVals, ffStates, f);
    auto foutPO = circuit_.getOutputs();
    auto foutFF = circuit_.getNextFFStates();
    for (int i = 0; i < static_cast<int>(freeOutputs.size()); ++i)
        if (foutPO[i] != freeOutputs[i]) return true;
    for (int i = 0; i < static_cast<int>(freeNextFF.size()); ++i)
        if (foutFF[i] != freeNextFF[i]) return true;
    return false;
}

ScanResult ScanController::run() {
    const auto &faults = circuit_.faults();
    int totalFaults = static_cast<int>(faults.size());
    std::unordered_set<int> detected;

    uint64_t lfsrState = cfg_.seed;
    if (lfsrState == 0) lfsrState = 1;

    int numPIs = circuit_.numPIs();
    int numFFs = circuit_.numFFs();

    std::ofstream outFile;
    bool writeOut = !cfg_.outputFile.empty();
    if (writeOut) {
        outFile.open(cfg_.outputFile);
        outFile << "pattern,detected_faults,fault_coverage\n";
    }

    for (int p = 0; p < cfg_.numPatterns; ++p) {
        std::vector<LogicVal> piVals, ffPattern;
        for (int i = 0; i < numPIs; ++i)
            piVals.push_back((lfsrNext(lfsrState) & 1) ? ONE : ZERO);
        for (int i = 0; i < numFFs; ++i)
            ffPattern.push_back((lfsrNext(lfsrState) & 1) ? ONE : ZERO);

        chain_.shiftIn(ffPattern);
        circuit_.simulate(piVals, chain_.states());
        auto freeOut = circuit_.getOutputs();
        auto freeFF  = circuit_.getNextFFStates();

        for (int fi = 0; fi < totalFaults; ++fi) {
            if (detected.count(fi)) continue;
            if (detects(piVals, chain_.states(), freeOut, freeFF, faults[fi]))
                detected.insert(fi);
        }

        if (writeOut) {
            double cov = totalFaults > 0 ? 100.0 * static_cast<int>(detected.size()) / totalFaults : 0.0;
            outFile << (p + 1) << "," << detected.size() << "," << cov << "\n";
        }
    }

    ScanResult result;
    result.totalFaults = totalFaults;
    result.detectedFaults = static_cast<int>(detected.size());
    result.faultCoverage = totalFaults > 0 ? 100.0 * result.detectedFaults / totalFaults : 0.0;
    result.patternsApplied = cfg_.numPatterns;
    return result;
}

} // namespace scan
