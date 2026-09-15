#pragma once

#include "types.hpp"
#include <string>
#include <vector>
namespace logicsim {
    std::string trimCopy(const std::string& input);
    std::string gname(int tp);
    std::vector<std::string> parseCommandArgs(std::string args);
    LogicValue toLogicValue(char c);
}
