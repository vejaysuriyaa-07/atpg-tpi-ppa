#pragma once

#include "../types.hpp"
#include <vector>
#include <string>
#include <unordered_set>


namespace logicsim {
    class Simulator;

    std::unordered_set<Fault> parallelFaultSim_impl(Simulator &simulator, bool atpg = false, std::vector<std::vector<unsigned>> patternMatrix = {}, std::unordered_set<Fault> faultList = {});
}
