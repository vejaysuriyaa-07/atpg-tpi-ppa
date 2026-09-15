#pragma once

#include "Command.hpp"
#include "Event.hpp"
#include "Node.hpp"
#include "types.hpp"
#include <string>
#include <vector>
namespace logicsim {
class Simulator {
private:
  State globalState;
  std::vector<Node> nodes;
  std::vector<Node *> primaryInputs;
  std::vector<Node *> primaryOutputs;
  int numNodes;
  int nPI;
  int nPO;
  bool done;
  char inFile[MAXLINE];
  Command commands[NUMFUNCS];
  std::string inp_name;
  std::string out_name;
  std::string patternInputPath;
  std::string patternOutputPath;
  EventTable eventTable;
  std::string commandArgs;

public:
  Simulator();

  // Nodes own their fan-in and fan-out arrays as raw pointers, so a copied
  // Simulator would share them and the first destructor to run would leave the
  // other holding freed memory. Everything takes it by reference anyway.
  Simulator(const Simulator &) = delete;
  Simulator &operator=(const Simulator &) = delete;

  std::map<int, std::vector<char>> results;
  State getGlobalState() const { return globalState; }
  void setGlobalState(State state) { globalState = state; }
  int isDone() const { return done; }
  void setCommandArgs(const std::string &args) { commandArgs = args; }
  const std::string &getCommandArgs() const { return commandArgs; }
  Command *getCommands() { return commands; }
  void cread();
  void pc();
  void help();
  void quit();
  void lev();
  void levelize(); // levelise only, writes nothing
  void clear();
  void simulationInit();
  void allocate();
  void eventDrivenSim();
  void circuitInit();
  void parallelFaultSim();
  void deductiveFaultSim(unsigned freq = 0);
  void randomTestPatternGen();
  void reducedFaultList();
  void testPatternCoverage();
  void dAlgorithm();
  void pathOrientedDecisionMaking();
  void scoap();
  void testPatternGenerator();
  void deterministicTestPatternCoverage();
  void testPointInsertion();
  void setInpName(const std::string &name) { inp_name = name; }
  const std::string &getInpName() const { return inp_name; }
  void setOutName(const std::string &name) { out_name = name; }
  const std::string &getOutName() const { return out_name; }
  void setNumNodes(int n) { numNodes = n; }
  int getNumNodes() const { return numNodes; }
  void setNPI(int n) { nPI = n; }
  int getNPI() const { return nPI; }
  void setNPO(int n) { nPO = n; }
  int getNPO() const { return nPO; }
  std::vector<Node> &getNodes() { return nodes; }
  void setNodes(const std::vector<Node> &ns) { nodes = ns; }
  std::vector<Node *> &getPrimaryInputs() { return primaryInputs; }
  void setPrimaryInputs(const std::vector<Node *> &pis) { primaryInputs = pis; }
  std::vector<Node *> &getPrimaryOutputs() { return primaryOutputs; }
  void setPrimaryOutputs(const std::vector<Node *> &pos) {
    primaryOutputs = pos;
  }
  EventTable &getEventTable() { return eventTable; }
  void setEventTable(const EventTable &et) { eventTable = et; }
  std::string getPatternInputPath() const { return patternInputPath; }
  void setPatternInputPath(const std::string &path) { patternInputPath = path; }
  std::string getPatternOutputPath() const { return patternOutputPath; }
  void setPatternOutputPath(const std::string &path) {
    patternOutputPath = path;
  }
};
} // namespace logicsim
