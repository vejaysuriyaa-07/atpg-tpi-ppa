#pragma once

#include "../types.hpp"
#include <string>

namespace logicsim {

// Per-cell area and delay, keyed by gate type and fanin count.
//
// The defaults below are scaled off the Nangate 45nm Open Cell Library X1
// drive strengths. They are a model, not silicon: nothing here has seen a
// real synthesis run. loadFromFile() swaps them for numbers parsed out of a
// Genus area report so the same selection code can run against real data on
// a machine that has the tools.
struct PpaModel {
  double invArea = 0.532;
  double bufArea = 0.798;
  double nand2Area = 0.798;
  double nor2Area = 0.798;
  double and2Area = 1.064;
  double or2Area = 1.064;
  double xor2Area = 1.596;

  double invDelay = 0.020;
  double bufDelay = 0.030;
  double nand2Delay = 0.030;
  double nor2Delay = 0.035;
  double and2Delay = 0.040;
  double or2Delay = 0.045;
  double xor2Delay = 0.070;

  std::string source = "analytical";

  // Reads a "key value" table. Accepts the output of tools/parse_genus.py,
  // which is what turns a real Genus report into this format.
  bool loadFromFile(const std::string &path, std::string &error);

  // Gates wider than two inputs are costed as a chain of 2-input cells,
  // which is what synthesis does to them anyway.
  double areaOf(GateType type, unsigned fanin) const;
  double delayOf(GateType type, unsigned fanin) const;
};

} // namespace logicsim
