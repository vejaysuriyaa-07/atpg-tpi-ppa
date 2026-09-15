#pragma once

#include <string>
#include <vector>

namespace logicsim {
class Simulator;

// Test point insertion driven by SCOAP testability numbers.
//
// The idea is the usual one: random patterns stall on nodes that are hard to
// control or hard to observe, so we bolt on extra hardware to fix exactly those
// nodes. A control point forces a value into a node through a new test pin, an
// observe point taps the node out to a new output. Both cost area, so the
// selection is budgeted.
//
// Usage: TPI <budget> <out_netlist.ckt> <report.csv> [-mode cp|op|mixed]

enum class TestPointKind { Control0, Control1, Observe };

struct TestPoint {
  int node;             // line number the point attaches to
  TestPointKind kind;
  double score;         // testability gain that got it selected
};

// Physical cost of the inserted hardware. Every number here comes from the
// analytical model in ppa.cpp unless a Cadence report was loaded first --
// see PpaModel::loadFromFile.
struct PpaEstimate {
  double baseArea = 0.0;      // um^2
  double addedArea = 0.0;
  double areaPercent = 0.0;
  double baseDelay = 0.0;     // ns, longest sensitizable-path estimate
  double newDelay = 0.0;
  double delayPercent = 0.0;
  double wirelengthProxy = 0.0;
  int addedGates = 0;
  int addedPis = 0;
  int addedPos = 0;
  std::string source = "analytical";
};

struct TpiResult {
  std::vector<TestPoint> points;
  PpaEstimate ppa;
  int nodesConsidered = 0;
};

int testPointInsertion_impl(Simulator &simulator);

} // namespace logicsim
