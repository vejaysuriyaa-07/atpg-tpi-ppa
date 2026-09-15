#pragma once
#include "Circuit.hpp"
#include "LFSR.hpp"
#include "MISR.hpp"
#include <string>
#include <vector>

namespace bist {

struct BISTConfig {
    int numPatterns = 1024;
    uint32_t seed = 0xACE1u;
    int misrWidth = 32;
    int lfsrWidth = 32;
    std::string outputFile;
};

struct BISTResult {
    int totalFaults;
    int detectedFaults;
    double faultCoverage;
    uint64_t misrSignature;
    int patternsApplied;
};

class BISTController {
public:
    BISTController(Circuit &circuit, const BISTConfig &cfg);
    BISTResult run();

private:
    Circuit &circuit_;
    BISTConfig cfg_;

    bool faultDetected(const std::vector<LogicVal> &inputs,
                       const std::vector<LogicVal> &faultFreeOutputs,
                       const Fault &fault);
};

} // namespace bist
