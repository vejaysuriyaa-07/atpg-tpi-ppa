#include "../include/Circuit.hpp"
#include "../include/ScanChain.hpp"
#include "../include/ScanController.hpp"
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>

static void usage(const char *prog) {
    std::cerr << "Usage: " << prog << " <circuit.bench> [-n num_patterns] [-seed value] [-o output_file]\n";
}

int main(int argc, char *argv[]) {
    if (argc < 2) { usage(argv[0]); return 1; }

    std::string path = argv[1];
    scan::ScanConfig cfg;

    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "-n") == 0 && i + 1 < argc)
            cfg.numPatterns = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "-seed") == 0 && i + 1 < argc)
            cfg.seed = static_cast<uint32_t>(std::stoul(argv[++i]));
        else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            cfg.outputFile = argv[++i];
        else { std::cerr << "Unknown argument: " << argv[i] << "\n"; usage(argv[0]); return 1; }
    }

    scan::Circuit circuit;
    std::string error;
    if (!circuit.loadFromFile(path, error)) {
        std::cerr << "Error: " << error << "\n";
        return 1;
    }

    scan::ScanChain chain(circuit.dffs());
    scan::ScanController controller(circuit, chain, cfg);
    scan::ScanResult result = controller.run();

    int pseudoPIs = circuit.numPIs() + circuit.numFFs();
    int pseudoPOs = circuit.numPOs() + circuit.numFFs();

    std::cout << "============================================\n";
    std::cout << "      Scan Chain Simulation Results\n";
    std::cout << "============================================\n";
    std::cout << "  Circuit:            " << circuit.name() << "\n";
    std::cout << "  Primary Inputs:     " << circuit.numPIs() << "\n";
    std::cout << "  Primary Outputs:    " << circuit.numPOs() << "\n";
    std::cout << "  Flip-Flops:         " << circuit.numFFs() << "\n";
    std::cout << "  Scan Chain Length:  " << chain.length() << "\n";
    std::cout << "  Pseudo-PIs:         " << pseudoPIs << "\n";
    std::cout << "  Pseudo-POs:         " << pseudoPOs << "\n";
    std::cout << "  Total Gates:        " << circuit.numGates() << "\n";
    std::cout << "  Patterns Applied:   " << result.patternsApplied << "\n";
    std::cout << "  Total Faults:       " << result.totalFaults << "\n";
    std::cout << "  Detected Faults:    " << result.detectedFaults << "\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Fault Coverage:     " << result.faultCoverage << "%\n";
    std::cout << "============================================\n";

    return 0;
}
