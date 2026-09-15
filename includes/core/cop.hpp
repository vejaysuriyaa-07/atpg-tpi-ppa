#pragma once

#include <vector>

namespace logicsim {
class Simulator;

// COP (controllability/observability program) signal probabilities.
//
// SCOAP counts how much work a deterministic ATPG run has to do to reach a
// node. That is the wrong question for random patterns: what matters there is
// how *likely* a random vector is to detect a fault, which is a probability,
// not a cost. COP computes that directly -- one forward pass for the odds a
// node sits at 1, one backward pass for the odds a flip on it reaches an
// output.
struct CopResult {
  std::vector<double> one;      // P(node = 1) under uniform random inputs
  std::vector<double> observe;  // P(a flip on the node reaches a PO)

  // A fault only shows up if the node is at the opposite value and the flip
  // makes it out, so the two probabilities multiply.
  double detect0(std::size_t i) const { return one[i] * observe[i]; }
  double detect1(std::size_t i) const { return (1.0 - one[i]) * observe[i]; }
};

// Requires a levelised circuit; the caller runs LEV first.
CopResult computeCop(Simulator &simulator);

} // namespace logicsim
