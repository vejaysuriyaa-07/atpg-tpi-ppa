#pragma once

#include <string>

namespace logicsim {
class Simulator;

// Syntactic Complexity Oriented Accessibility Analysis (SCOAP) implementation
// Computes controllability and observability metrics for testability analysis
// Usage: SCOAP <output_file>
// When atpg=true, skips file output (used internally by TPG)
int syntacticComplexityOrientedAccessibilityAnalysis_impl(
    Simulator &simulator, const std::string &outFileName, bool atpg = false);

} // namespace logicsim
