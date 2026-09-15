#pragma once
#include "Types.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace bist {

class Circuit {
public:
    bool loadFromFile(const std::string &path, std::string &error);

    void simulate(const std::vector<LogicVal> &inputVals);
    void simulateWithFault(const std::vector<LogicVal> &inputVals, int faultGateId, int stuckAt);

    std::vector<LogicVal> getOutputs() const;

    int numPIs() const { return static_cast<int>(pis_.size()); }
    int numPOs() const { return static_cast<int>(pos_.size()); }
    int numGates() const { return static_cast<int>(gates_.size()); }

    const std::vector<Fault> &faults() const { return faults_; }
    std::string name() const { return name_; }

private:
    std::string name_;
    std::vector<Gate> gates_;
    std::vector<int> pis_;
    std::vector<int> pos_;
    std::unordered_map<int, int> idToIdx_;
    std::vector<Fault> faults_;

    void levelize();
    void buildFaultList();
    LogicVal evalGate(const Gate &g) const;
    void applyInputs(const std::vector<LogicVal> &inputVals);
};

} // namespace bist
