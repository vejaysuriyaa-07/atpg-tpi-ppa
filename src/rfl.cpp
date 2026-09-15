#include "../includes/core/rfl.hpp"
#include "../includes/Simulator.hpp"   // <-- add this
#include <iostream>
#include <string>
#include <fstream>

std::vector<std::string> reducedFaultList_impl(logicsim::Simulator &sim,
                           const std::string &outFileName, bool atpg) {
    std::vector<std::string> faults;
    faults.reserve(static_cast<std::size_t>(sim.getNPI()) * 2 +
                   static_cast<std::size_t>(sim.getNumNodes())); // rough guess

    auto &nodes = sim.getNodes();
    auto &primaryInputs = sim.getPrimaryInputs();

    // 1) Primary input faults
    for (auto *pi : primaryInputs) {
        faults.emplace_back(std::to_string(pi->getNum()) + "@0");
        faults.emplace_back(std::to_string(pi->getNum()) + "@1");
    }

    // 2) Fanout branch faults (simple FB detection)
    for (auto &n : nodes) {
        auto **ups = n.getUnodes();
        int fin = n.getFin();

        for (int j = 0; j < fin; ++j) {
            auto *up = ups[j];
            if (up && up->getFout() > 1) {
                faults.emplace_back(std::to_string(n.getNum()) + "@0");
                faults.emplace_back(std::to_string(n.getNum()) + "@1");
                break; // stop after first FB upstream
            }
        }
    }
    if (!atpg) {
        std::ofstream out(outFileName);
        if (!out.is_open()) {
            std::cerr << "Error: cannot open " << outFileName << "\n";
        }

        for (const auto &fault : faults) {
            out << fault << '\n';
        }
        out.flush();
        std::cout << "Reduced Fault List written to: " << outFileName << "\n";
    }
    return faults;
}
