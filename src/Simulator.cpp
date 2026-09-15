#include "../includes/Simulator.hpp"
#include "../includes/core/core_func.hpp"
#include "../includes/utils.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

namespace logicsim {
Simulator::Simulator() {
  globalState = State::EXEC;
  numNodes = nPI = nPO = 0;
  done = 0;
  inFile[0] = '\0';
  inp_name = "";
  out_name = "";
  patternInputPath = "";
  patternOutputPath = "";
  commandArgs.clear();
  commands[0].registerCommand("READ", [this]() { this->cread(); }, State::EXEC);
  commands[1].registerCommand("PC", [this]() { this->pc(); }, State::CKTLD);
  commands[2].registerCommand("HELP", [this]() { this->help(); }, State::EXEC);
  commands[3].registerCommand("QUIT", [this]() { this->quit(); }, State::EXEC);
  commands[4].registerCommand("LEV", [this]() { this->lev(); }, State::CKTLD);
  commands[5].registerCommand(
      "LOGICSIM", [this]() { this->eventDrivenSim(); }, State::CKTLD);
  commands[6].registerCommand(
      "PFS", [this]() { this->parallelFaultSim(); }, State::CKTLD);
  commands[7].registerCommand(
      "DFS", [this]() { this->deductiveFaultSim(); }, State::CKTLD);
  commands[8].registerCommand(
      "RTPG", [this]() { this->randomTestPatternGen(); }, State::CKTLD);
  commands[9].registerCommand(
      "RFL", [this]() { this->reducedFaultList(); }, State::CKTLD);
  commands[10].registerCommand(
      "TPFC", [this]() { this->testPatternCoverage(); }, State::CKTLD);
  commands[11].registerCommand(
      "DALG", [this]() { this->dAlgorithm(); }, State::CKTLD);
  commands[12].registerCommand(
      "PODEM", [this]() { this->pathOrientedDecisionMaking(); }, State::CKTLD);
  commands[13].registerCommand(
      "SCOAP", [this]() { this->scoap(); }, State::CKTLD);
  commands[14].registerCommand(
      "TPG", [this]() { this->testPatternGenerator(); }, State::CKTLD);
  commands[15].registerCommand(
      "DTPFC", [this]() { this->deterministicTestPatternCoverage(); },
      State::CKTLD);
  commands[16].registerCommand(
      "TPI", [this]() { this->testPointInsertion(); }, State::CKTLD);
}

void Simulator::cread() { cread_impl(*this); }
void Simulator::pc() { pc_impl(*this); }
void Simulator::help() {
  printf("READ filename - ");
  printf("read in circuit file and creat all data structures\n");
  printf("PC - ");
  printf("print circuit information\n");
  printf("HELP - ");
  printf("print this help information\n");
  printf("QUIT - ");
  printf("stop and exit\n");
  printf("LEV - ");
  printf("levelize circuit\n");
  printf("LOGICSIM <pattern_file> <output_file> - ");
  printf("perform event-driven logic simulation\n");
  printf("RFL <output_fl_file> - ");
  printf("generate reduced fault list and write to output file\n");
}
void Simulator::quit() {
  Simulator::done = 1;
  nodes.clear();
  primaryInputs.clear();
  primaryOutputs.clear();
  commandArgs.clear();
  eventTable.clear();
  results.clear();
  patternInputPath.clear();
  patternOutputPath.clear();
}
void Simulator::lev() { lev_impl(*this); }
void Simulator::levelize() { lev_impl(*this, false); }
void Simulator::clear() {
  Simulator::nodes.clear();
  Simulator::primaryInputs.clear();
  Simulator::primaryOutputs.clear();
  Simulator::globalState = State::EXEC;
  commandArgs.clear();
  simulationInit();
}
void Simulator::simulationInit() {
  results.clear();
  eventTable.clear();
  patternInputPath.clear();
  patternOutputPath.clear();
}
void Simulator::allocate() {
  nodes.clear();
  primaryInputs.clear();
  primaryOutputs.clear();

  nodes.resize(numNodes);
  primaryInputs.resize(nPI, nullptr);
  primaryOutputs.resize(nPO, nullptr);

  for (int i = 0; i < numNodes; i++) {
    nodes[i].setIndx(i);
    nodes[i].setFin(0);
    nodes[i].setFout(0);
  }
}
void Simulator::eventDrivenSim() { eventDrivenSim_impl(*this); }
void Simulator::parallelFaultSim() { parallelFaultSim_impl(*this); }
void Simulator::deductiveFaultSim(unsigned freq) {
  deductiveFaultSim_impl(*this, freq);
}
void Simulator::randomTestPatternGen() { randomTestPatternGen_impl(*this); }
void Simulator::circuitInit() {
  // Initialize all nodes to a default logic value, e.g., UNDEF
  for (auto &node : nodes) {
    for (unsigned i = 0; i < SystemWidth; ++i) {
      node.setValue(LogicValue::UNDEF, i);
    }
    node.clearFaultList();
  }
}
void Simulator::reducedFaultList() {
  // Get arguments passed from the command line
  if (commandArgs.empty()) {
    std::cerr << "Usage: RFL <output-fl-file>\n";
    return;
  }

  // Extract the filename argument
  std::istringstream iss(commandArgs);
  std::string outputFile;
  iss >> outputFile;

  // Call the actual RFL function implemented in rfl.cpp
  ::reducedFaultList_impl(*this, outputFile);
}
void Simulator::testPatternCoverage() { testPatternCoverage_impl(*this); }
void Simulator::dAlgorithm() { dAlgorithm_impl(*this); }
void Simulator::pathOrientedDecisionMaking() {
  // Get arguments passed from the command line
  if (commandArgs.empty()) {
    std::cerr << "Usage: PODEM <fault_node> <fault_value> <output_file>\n";
    return;
  }

  // Extract the output filename (last argument)
  std::istringstream iss(commandArgs);
  std::string arg1, arg2, outputFile;
  iss >> arg1 >> arg2 >> outputFile;

  if (outputFile.empty()) {
    std::cerr << "Usage: PODEM <fault_node> <fault_value> <output_file>\n";
    return;
  }

  // Call the actual PODEM function implemented in podem.cpp
  pathOrientedDecisionMaking_impl(*this, outputFile);
}
void Simulator::scoap() {
  // Get arguments passed from the command line
  if (commandArgs.empty()) {
    std::cerr << "Usage: SCOAP <output_file>\n";
    return;
  }

  // Extract the output filename argument
  std::istringstream iss(commandArgs);
  std::string outputFile;
  iss >> outputFile;

  // Call the actual SCOAP function implemented in scoap.cpp
  syntacticComplexityOrientedAccessibilityAnalysis_impl(*this, outputFile);
}

void Simulator::testPatternGenerator() { testPatternGenerator_impl(*this); }
void Simulator::deterministicTestPatternCoverage() {
  deterministicTestPatternCoverage_impl(*this);
}
void Simulator::testPointInsertion() { testPointInsertion_impl(*this); }
} // namespace logicsim
