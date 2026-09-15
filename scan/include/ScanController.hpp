#pragma once
#include "Circuit.hpp"
#include "ScanChain.hpp"
#include <cstdint>
#include <string>

namespace scan {

struct ScanConfig {
    int numPatterns = 1024;
    uint32_t seed = 0xACE1u;
    std::string outputFile;
};

struct ScanResult {
    int totalFaults;
    int detectedFaults;
    double faultCoverage;
    int patternsApplied;
};

class ScanController {
public:
    ScanController(Circuit &circuit, ScanChain &chain, const ScanConfig &cfg);
    ScanResult run();

private:
    Circuit &circuit_;
    ScanChain &chain_;
    ScanConfig cfg_;

    bool detects(const std::vector<LogicVal> &piVals,
                 const std::vector<LogicVal> &ffStates,
                 const std::vector<LogicVal> &freeOutputs,
                 const std::vector<LogicVal> &freeNextFF,
                 const Fault &f);
};

} // namespace scan
