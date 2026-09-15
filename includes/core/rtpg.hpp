#pragma once
#include <vector>
#include <string>
namespace logicsim {
    class Simulator;
    std::vector<std::vector<unsigned>> randomTestPatternGen_impl(Simulator &simulator, bool atpg = false, unsigned tpCount_arg = 0, std::string mode_arg = "b");
    static std::vector<std::string> parseCommandArgs(Simulator &simulator);
}