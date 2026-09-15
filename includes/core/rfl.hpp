#pragma once
#include <string>
#include <vector>

// Forward-declare Simulator inside its namespace so we can use it in the signature.
namespace logicsim { class Simulator; }

// Free function (global namespace) that implements RFL logic.
std::vector<std::string> reducedFaultList_impl(logicsim::Simulator& sim, const std::string& outFileName, bool atpg = false);
