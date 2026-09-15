#include "core/ppa.hpp"
#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>

using namespace logicsim;

namespace {

TEST(PpaModel, WiringCostsNothing) {
  PpaModel m;
  EXPECT_DOUBLE_EQ(m.areaOf(BRCH, 1), 0.0);
  EXPECT_DOUBLE_EQ(m.areaOf(IPT, 0), 0.0);
  EXPECT_DOUBLE_EQ(m.delayOf(BRCH, 1), 0.0);
}

TEST(PpaModel, TwoInputGatesUseTheCellArea) {
  PpaModel m;
  EXPECT_DOUBLE_EQ(m.areaOf(AND, 2), m.and2Area);
  EXPECT_DOUBLE_EQ(m.areaOf(NAND, 2), m.nand2Area);
  EXPECT_DOUBLE_EQ(m.areaOf(NOT, 1), m.invArea);
}

TEST(PpaModel, WideGatesCostAChainOfCells) {
  PpaModel m;
  // Four inputs decompose into three two-input cells.
  EXPECT_DOUBLE_EQ(m.areaOf(AND, 4), m.and2Area * 3.0);
  EXPECT_DOUBLE_EQ(m.areaOf(OR, 3), m.or2Area * 2.0);
}

TEST(PpaModel, WideGateDelayGrowsLogarithmically) {
  PpaModel m;
  // A balanced tree, so four inputs is two stages, not three.
  EXPECT_DOUBLE_EQ(m.delayOf(AND, 4), m.and2Delay * 2.0);
  EXPECT_DOUBLE_EQ(m.delayOf(AND, 2), m.and2Delay);
  EXPECT_LT(m.delayOf(AND, 8), m.and2Delay * 8.0);
}

TEST(PpaModel, FileOverridesDefaults) {
  const char *path = "ppa_test_model.txt";
  {
    std::ofstream f(path);
    f << "# swapped in from a synthesis report\n";
    f << "and2_area 2.5\n";
    f << "and2_delay 0.11\n";
  }

  PpaModel m;
  std::string err;
  ASSERT_TRUE(m.loadFromFile(path, err)) << err;
  EXPECT_DOUBLE_EQ(m.areaOf(AND, 2), 2.5);
  EXPECT_DOUBLE_EQ(m.delayOf(AND, 2), 0.11);
  // Untouched keys keep their defaults.
  EXPECT_DOUBLE_EQ(m.areaOf(NOT, 1), PpaModel{}.invArea);
  // And the report records where the numbers came from.
  EXPECT_NE(m.source.find(path), std::string::npos);
  std::remove(path);
}

TEST(PpaModel, RejectsJunk) {
  const char *path = "ppa_bad_model.txt";
  {
    std::ofstream f(path);
    f << "not_a_real_key 1.0\n";
  }
  PpaModel m;
  std::string err;
  EXPECT_FALSE(m.loadFromFile(path, err));
  EXPECT_FALSE(err.empty());
  std::remove(path);
}

TEST(PpaModel, MissingFileIsAnError) {
  PpaModel m;
  std::string err;
  EXPECT_FALSE(m.loadFromFile("definitely_not_here.txt", err));
}

} // namespace
