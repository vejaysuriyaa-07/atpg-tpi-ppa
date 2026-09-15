#include "../include/BISTController.hpp"
#include <fstream>
#include <unordered_set>

namespace bist {

BISTController::BISTController(Circuit &circuit, const BISTConfig &cfg)
    : circuit_(circuit), cfg_(cfg) {}

bool BISTController::faultDetected(const std::vector<LogicVal> &inputs,
                                    const std::vector<LogicVal> &faultFreeOutputs,
                                    const Fault &fault) {
    circuit_.simulateWithFault(inputs, fault.gateId, fault.stuckAt);
    std::vector<LogicVal> faultedOutputs = circuit_.getOutputs();

    for (int i = 0; i < static_cast<int>(faultFreeOutputs.size()); ++i) {
        if (faultedOutputs[i] != faultFreeOutputs[i]) return true;
    }
    return false;
}

BISTResult BISTController::run() {
    LFSR lfsr(cfg_.lfsrWidth, cfg_.seed);
    MISR misr(cfg_.misrWidth);

    const std::vector<Fault> &faults = circuit_.faults();
    int totalFaults = static_cast<int>(faults.size());
    std::unordered_set<int> detected;

    std::ofstream outFile;
    bool writeOutput = !cfg_.outputFile.empty();
    if (writeOutput) {
        outFile.open(cfg_.outputFile);
        outFile << "pattern,detected_faults,fault_coverage\n";
    }

    for (int p = 0; p < cfg_.numPatterns; ++p) {
        std::vector<int> bits = lfsr.generatePattern(circuit_.numPIs());

        std::vector<LogicVal> inputs;
        inputs.reserve(bits.size());
        for (int b : bits) inputs.push_back(b ? ONE : ZERO);

        circuit_.simulate(inputs);
        std::vector<LogicVal> faultFreeOut = circuit_.getOutputs();

        std::vector<int> outBits;
        outBits.reserve(faultFreeOut.size());
        for (LogicVal v : faultFreeOut) outBits.push_back(v == ONE ? 1 : 0);
        misr.compress(outBits);

        for (int fi = 0; fi < totalFaults; ++fi) {
            if (detected.count(fi)) continue;
            if (faultDetected(inputs, faultFreeOut, faults[fi])) {
                detected.insert(fi);
            }
        }

        if (writeOutput) {
            double cov = totalFaults > 0 ? (100.0 * static_cast<int>(detected.size()) / totalFaults) : 0.0;
            outFile << (p + 1) << "," << detected.size() << "," << cov << "\n";
        }
    }

    BISTResult result;
    result.totalFaults = totalFaults;
    result.detectedFaults = static_cast<int>(detected.size());
    result.faultCoverage = totalFaults > 0 ? (100.0 * result.detectedFaults / totalFaults) : 0.0;
    result.misrSignature = misr.signature();
    result.patternsApplied = cfg_.numPatterns;
    return result;
}

} // namespace bist
