#pragma once
#include "Types.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace scan {

class Circuit {
public:
    bool loadFromFile(const std::string &path, std::string &error);

    void simulate(const std::vector<LogicVal> &piVals, const std::vector<LogicVal> &ffStates);
    void simulateWithFault(const std::vector<LogicVal> &piVals, const std::vector<LogicVal> &ffStates, const Fault &f);

    std::vector<LogicVal> getOutputs() const;
    std::vector<LogicVal> getNextFFStates() const;

    int numPIs() const { return static_cast<int>(pis_.size()); }
    int numPOs() const { return static_cast<int>(pos_.size()); }
    int numFFs() const { return static_cast<int>(dffs_.size()); }
    int numGates() const { return static_cast<int>(gates_.size()); }

    const std::vector<DFF> &dffs() const { return dffs_; }
    const std::vector<Fault> &faults() const { return faults_; }
    std::string name() const { return name_; }

private:
    std::string name_;
    std::vector<Gate> gates_;
    std::vector<std::string> pis_;
    std::vector<std::string> pos_;
    std::vector<DFF> dffs_;
    std::unordered_map<std::string, int> nameToIdx_;
    std::vector<Fault> faults_;

    void levelize();
    void buildFaultList();
    LogicVal evalGate(const Gate &g) const;
    void setSources(const std::vector<LogicVal> &piVals, const std::vector<LogicVal> &ffStates);
};

} // namespace scan
