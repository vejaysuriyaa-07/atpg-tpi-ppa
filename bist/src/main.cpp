#include "../include/Circuit.hpp"
#include "../include/LFSR.hpp"
#include "../include/MISR.hpp"
#include "../include/BISTController.hpp"
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

static void printUsage(const char *prog) {
    std::cerr << "Usage: " << prog << " <circuit.ckt> [-n num_patterns] [-seed value] [-misr width] [-o output_file]\n";
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string circuitPath = argv[1];
    bist::BISTConfig cfg;

    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            cfg.numPatterns = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "-seed") == 0 && i + 1 < argc) {
            cfg.seed = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (std::strcmp(argv[i], "-misr") == 0 && i + 1 < argc) {
            cfg.misrWidth = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            cfg.outputFile = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << argv[i] << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    bist::Circuit circuit;
    std::string error;
    if (!circuit.loadFromFile(circuitPath, error)) {
        std::cerr << "Error loading circuit: " << error << "\n";
        return 1;
    }

    cfg.lfsrWidth = 32;

    bist::BISTController controller(circuit, cfg);
    bist::BISTResult result = controller.run();

    std::cout << "============================================\n";
    std::cout << "         BIST Simulation Results\n";
    std::cout << "============================================\n";
    std::cout << "  Circuit:            " << circuit.name() << "\n";
    std::cout << "  Primary Inputs:     " << circuit.numPIs() << "\n";
    std::cout << "  Primary Outputs:    " << circuit.numPOs() << "\n";
    std::cout << "  Total Gates:        " << circuit.numGates() << "\n";
    std::cout << "  LFSR Width:         " << cfg.lfsrWidth << "\n";
    std::cout << "  MISR Width:         " << cfg.misrWidth << "\n";
    std::cout << "  Patterns Applied:   " << result.patternsApplied << "\n";
    std::cout << "  Total Faults:       " << result.totalFaults << "\n";
    std::cout << "  Detected Faults:    " << result.detectedFaults << "\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Fault Coverage:     " << result.faultCoverage << "%\n";
    std::cout << "  MISR Signature:     0x" << std::hex << std::uppercase
              << std::setfill('0') << std::setw(8) << result.misrSignature << "\n";
    std::cout << std::dec;
    std::cout << "============================================\n";

    return 0;
}
