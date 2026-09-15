#include "Simulator.hpp"
#include "core/cop.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace logicsim;

namespace {

// c17 is small enough to check the arithmetic by hand, which is the point of
// using it here. The simulator is built in place because it cannot be copied.
class CopTest : public ::testing::Test {
protected:
  Simulator sim;
  void SetUp() override {
    sim.setCommandArgs(std::string(CKT_DIR) + "/c17.ckt");
    sim.cread();
    sim.levelize();
  }
};

Node *byNum(Simulator &sim, unsigned num) {
  for (auto &n : sim.getNodes())
    if (n.getNum() == num)
      return &n;
  return nullptr;
}

TEST_F(CopTest, InputsAreUnbiased) {
  CopResult cop = computeCop(sim);
  for (auto *pi : sim.getPrimaryInputs())
    EXPECT_DOUBLE_EQ(cop.one[pi->getIndx()], 0.5);
}

TEST_F(CopTest, NandOfTwoFairInputs) {
  CopResult cop = computeCop(sim);
  // Line 10 is NAND(1, 8) and both sides are unbiased, so it sits at 1
  // three times in four.
  Node *n10 = byNum(sim, 10);
  ASSERT_NE(n10, nullptr);
  EXPECT_DOUBLE_EQ(cop.one[n10->getIndx()], 0.75);
}

TEST_F(CopTest, OutputsAreFullyObservable) {
  CopResult cop = computeCop(sim);
  for (auto *po : sim.getPrimaryOutputs())
    EXPECT_DOUBLE_EQ(cop.observe[po->getIndx()], 1.0);
}

TEST_F(CopTest, EverythingStaysAProbability) {
  CopResult cop = computeCop(sim);
  for (std::size_t i = 0; i < cop.one.size(); ++i) {
    EXPECT_GE(cop.one[i], 0.0);
    EXPECT_LE(cop.one[i], 1.0);
    EXPECT_GE(cop.observe[i], 0.0);
    EXPECT_LE(cop.observe[i], 1.0);
  }
}

// Observability can only shrink as you move back from the outputs, since every
// extra gate is another chance to mask the flip.
TEST_F(CopTest, ObservabilityDoesNotGrowUpstream) {
  CopResult cop = computeCop(sim);
  for (auto &n : sim.getNodes()) {
    if (n.getFin() == 0 || n.getType() == BRCH)
      continue;
    Node **un = n.getUnodes();
    for (unsigned k = 0; k < n.getFin(); ++k)
      EXPECT_LE(cop.observe[un[k]->getIndx()], cop.observe[n.getIndx()] + 1e-12)
          << "line " << un[k]->getNum() << " feeding " << n.getNum();
  }
}

TEST_F(CopTest, DetectionOddsSplitByStuckAtValue) {
  CopResult cop = computeCop(sim);
  Node *n10 = byNum(sim, 10);
  ASSERT_NE(n10, nullptr);
  const std::size_t i = n10->getIndx();
  // A stuck-at-0 only shows when the line should have been 1.
  EXPECT_DOUBLE_EQ(cop.detect0(i), cop.one[i] * cop.observe[i]);
  EXPECT_DOUBLE_EQ(cop.detect1(i), (1.0 - cop.one[i]) * cop.observe[i]);
}

} // namespace
