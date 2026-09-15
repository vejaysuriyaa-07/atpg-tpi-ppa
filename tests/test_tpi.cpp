#include "Simulator.hpp"
#include "core/tpi.hpp"
#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

using namespace logicsim;

namespace {

struct Counts {
  int nodes = 0;
  int pis = 0;
  int pos = 0;
};

// Counts straight out of the file rather than through the simulator, so a
// netlist that cannot be parsed fails loudly instead of quietly reading back
// as something else.
Counts countFile(const std::string &path) {
  Counts c;
  std::ifstream f(path);
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty())
      continue;
    std::istringstream ss(line);
    int ntype = 0, id = 0;
    if (!(ss >> ntype >> id))
      continue;
    ++c.nodes;
    if (ntype == static_cast<int>(PI))
      ++c.pis;
    else if (ntype == static_cast<int>(PO))
      ++c.pos;
  }
  return c;
}

std::string ckt(const std::string &name) {
  return std::string(CKT_DIR) + "/" + name;
}

class Tpi : public ::testing::Test {
protected:
  Simulator sim;
  std::string out = "tpi_test_out.ckt";
  std::string rep = "tpi_test_out.csv";

  void load(const std::string &name) {
    sim.setCommandArgs(ckt(name));
    sim.cread();
    sim.levelize();
  }

  void insert(const std::string &args) {
    sim.setCommandArgs(args);
    sim.testPointInsertion();
  }

  int pointCount() {
    std::ifstream f(rep + ".points");
    std::string line;
    int n = -1; // discount the header
    while (std::getline(f, line))
      if (!line.empty())
        ++n;
    return n;
  }

  void TearDown() override {
    std::remove(out.c_str());
    std::remove(rep.c_str());
    std::remove((rep + ".points").c_str());
  }
};

TEST_F(Tpi, StaysWithinBudget) {
  load("c432.ckt");
  insert("8 " + out + " " + rep + " -mode mixed");
  EXPECT_LE(pointCount(), 8);
  EXPECT_GT(pointCount(), 0);
}

TEST_F(Tpi, OutputParsesAndGrows) {
  load("c17.ckt");
  const Counts before = countFile(ckt("c17.ckt"));
  insert("4 " + out + " " + rep + " -mode mixed");
  const Counts after = countFile(out);

  EXPECT_GT(after.nodes, before.nodes);
  EXPECT_GE(after.pis, before.pis);
  EXPECT_GE(after.pos, before.pos);
}

// The transformed netlist has to survive a real read, since that is how every
// downstream tool consumes it.
TEST_F(Tpi, TransformedNetlistReloads) {
  load("c432.ckt");
  insert("8 " + out + " " + rep + " -mode mixed");

  Simulator reread;
  reread.setCommandArgs(out);
  reread.cread();
  EXPECT_EQ(reread.getGlobalState(), State::CKTLD);
  EXPECT_GT(reread.getNumNodes(), 0);
  EXPECT_EQ(static_cast<int>(reread.getPrimaryInputs().size()), reread.getNPI());
  EXPECT_EQ(static_cast<int>(reread.getPrimaryOutputs().size()), reread.getNPO());
  for (auto *p : reread.getPrimaryInputs())
    ASSERT_NE(p, nullptr);
  for (auto *p : reread.getPrimaryOutputs())
    ASSERT_NE(p, nullptr);
}

TEST_F(Tpi, ObservePointsAddOutputsOnly) {
  load("c432.ckt");
  const Counts before = countFile(ckt("c432.ckt"));
  insert("8 " + out + " " + rep + " -mode op");
  const Counts after = countFile(out);

  EXPECT_GT(after.pos, before.pos);
  EXPECT_EQ(after.pis, before.pis) << "an observe point needs no new input";
}

TEST_F(Tpi, ControlPointsAddInputsOnly) {
  load("c432.ckt");
  const Counts before = countFile(ckt("c432.ckt"));
  insert("8 " + out + " " + rep + " -mode cp -weight 3");
  const Counts after = countFile(out);

  EXPECT_GT(after.pis, before.pis);
  EXPECT_EQ(after.pos, before.pos) << "a control point needs no new output";
}

// weight n gates the enable through n pins, so the pin count scales with it.
TEST_F(Tpi, EnableWeightScalesInputCount) {
  load("c432.ckt");
  const Counts base = countFile(ckt("c432.ckt"));

  insert("4 " + out + " " + rep + " -mode cp -weight 1");
  const int light = countFile(out).pis - base.pis;

  load("c432.ckt");
  insert("4 " + out + " " + rep + " -mode cp -weight 3");
  const int heavy = countFile(out).pis - base.pis;

  EXPECT_GT(heavy, light);
}

TEST_F(Tpi, ReportRecordsCostAndProvenance) {
  load("c432.ckt");
  insert("8 " + out + " " + rep + " -mode mixed");

  std::ifstream f(rep);
  std::string header, row;
  ASSERT_TRUE(std::getline(f, header));
  ASSERT_TRUE(std::getline(f, row));
  EXPECT_NE(header.find("area_pct"), std::string::npos);
  EXPECT_NE(header.find("ppa_source"), std::string::npos);
  // Nothing has been swapped in, so the numbers are the built-in model's.
  EXPECT_NE(row.find("analytical"), std::string::npos);
}

TEST_F(Tpi, RejectsBadArguments) {
  load("c17.ckt");
  insert("0 " + out + " " + rep);
  std::ifstream none(out);
  EXPECT_FALSE(none.good()) << "a zero budget should not produce a netlist";

  insert("4 " + out + " " + rep + " -mode sideways");
  std::ifstream bad(out);
  EXPECT_FALSE(bad.good()) << "an unknown mode should be refused";
}

TEST_F(Tpi, BothMetricsSelectPoints) {
  load("c432.ckt");
  insert("8 " + out + " " + rep + " -metric cop");
  EXPECT_GT(pointCount(), 0);

  load("c432.ckt");
  insert("8 " + out + " " + rep + " -metric scoap");
  EXPECT_GT(pointCount(), 0);
}

} // namespace
