#include "../include/Circuit.hpp"
#include <algorithm>
#include <fstream>
#include <queue>
#include <sstream>

namespace bist {

bool Circuit::loadFromFile(const std::string &path, std::string &error) {
    std::ifstream f(path);
    if (!f.is_open()) {
        error = "Cannot open file: " + path;
        return false;
    }

    std::string base = path;
    auto slash = base.rfind('/');
    if (slash != std::string::npos) base = base.substr(slash + 1);
    auto dot = base.rfind('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    name_ = base;

    gates_.clear();
    idToIdx_.clear();
    pis_.clear();
    pos_.clear();

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        int ntVal, id;
        if (!(ss >> ntVal >> id)) continue;

        Gate g;
        g.id = id;
        g.ntype = static_cast<NodeType>(ntVal);
        g.value = UNKNOWN;
        g.level = -1;

        if (g.ntype == FB) {
            int gtVal;
            if (!(ss >> gtVal)) {
                error = "Malformed FB gate " + std::to_string(id);
                return false;
            }
            g.gtype = static_cast<GateType>(gtVal);
            g.fanout = 1;
            g.fanin = 1;
            int inId;
            while (ss >> inId) g.inputs.push_back(inId);
        } else {
            int gtVal, fo, fi;
            if (!(ss >> gtVal >> fo >> fi)) {
                error = "Malformed gate " + std::to_string(id);
                return false;
            }
            g.gtype = static_cast<GateType>(gtVal);
            g.fanout = fo;
            g.fanin = fi;
            for (int i = 0; i < fi; ++i) {
                int inId;
                if (!(ss >> inId)) {
                    error = "Missing fanin for gate " + std::to_string(id);
                    return false;
                }
                g.inputs.push_back(inId);
            }
        }

        int idx = static_cast<int>(gates_.size());
        idToIdx_[id] = idx;
        gates_.push_back(std::move(g));
    }

    for (int i = 0; i < static_cast<int>(gates_.size()); ++i) {
        if (gates_[i].ntype == PI) pis_.push_back(i);
        else if (gates_[i].ntype == PO) pos_.push_back(i);
    }

    levelize();
    buildFaultList();
    return true;
}

void Circuit::levelize() {
    std::unordered_map<int, int> indegree;
    for (auto &g : gates_) indegree[g.id] = 0;

    for (auto &g : gates_)
        for (int inId : g.inputs)
            if (idToIdx_.count(inId))
                indegree[g.id]++;

    std::unordered_map<int, std::vector<int>> fanout;
    for (auto &g : gates_)
        for (int inId : g.inputs)
            fanout[inId].push_back(g.id);

    std::queue<int> q;
    for (auto &g : gates_) {
        if (g.inputs.empty()) {
            g.level = 0;
            q.push(g.id);
        }
    }

    while (!q.empty()) {
        int cur = q.front(); q.pop();
        Gate &cg = gates_[idToIdx_[cur]];
        for (int outId : fanout[cur]) {
            Gate &og = gates_[idToIdx_[outId]];
            og.level = std::max(og.level, cg.level + 1);
            indegree[outId]--;
            if (indegree[outId] == 0) q.push(outId);
        }
    }

    std::stable_sort(gates_.begin(), gates_.end(), [](const Gate &a, const Gate &b) {
        return a.level < b.level;
    });

    idToIdx_.clear();
    for (int i = 0; i < static_cast<int>(gates_.size()); ++i)
        idToIdx_[gates_[i].id] = i;

    pis_.clear(); pos_.clear();
    for (int i = 0; i < static_cast<int>(gates_.size()); ++i) {
        if (gates_[i].ntype == PI) pis_.push_back(i);
        else if (gates_[i].ntype == PO) pos_.push_back(i);
    }
}

void Circuit::buildFaultList() {
    faults_.clear();
    for (auto &g : gates_) {
        faults_.push_back({g.id, 0});
        faults_.push_back({g.id, 1});
    }
}

LogicVal Circuit::evalGate(const Gate &g) const {
    if (g.inputs.empty()) return g.value;

    auto getVal = [&](int id) -> LogicVal {
        auto it = idToIdx_.find(id);
        if (it == idToIdx_.end()) return UNKNOWN;
        return gates_[it->second].value;
    };

    switch (g.gtype) {
        case IPT:
        case BRCH:
        case BUF: {
            return getVal(g.inputs[0]);
        }
        case NOT: {
            LogicVal v = getVal(g.inputs[0]);
            if (v == ONE) return ZERO;
            if (v == ZERO) return ONE;
            return UNKNOWN;
        }
        case AND: {
            bool anyZero = false, anyUnknown = false;
            for (int id : g.inputs) {
                LogicVal v = getVal(id);
                if (v == ZERO) anyZero = true;
                else if (v == UNKNOWN) anyUnknown = true;
            }
            if (anyZero) return ZERO;
            if (anyUnknown) return UNKNOWN;
            return ONE;
        }
        case NAND: {
            bool anyZero = false, anyUnknown = false;
            for (int id : g.inputs) {
                LogicVal v = getVal(id);
                if (v == ZERO) anyZero = true;
                else if (v == UNKNOWN) anyUnknown = true;
            }
            if (anyZero) return ONE;
            if (anyUnknown) return UNKNOWN;
            return ZERO;
        }
        case OR: {
            bool anyOne = false, anyUnknown = false;
            for (int id : g.inputs) {
                LogicVal v = getVal(id);
                if (v == ONE) anyOne = true;
                else if (v == UNKNOWN) anyUnknown = true;
            }
            if (anyOne) return ONE;
            if (anyUnknown) return UNKNOWN;
            return ZERO;
        }
        case NOR: {
            bool anyOne = false, anyUnknown = false;
            for (int id : g.inputs) {
                LogicVal v = getVal(id);
                if (v == ONE) anyOne = true;
                else if (v == UNKNOWN) anyUnknown = true;
            }
            if (anyOne) return ZERO;
            if (anyUnknown) return UNKNOWN;
            return ONE;
        }
        case XOR: {
            int ones = 0; bool anyUnknown = false;
            for (int id : g.inputs) {
                LogicVal v = getVal(id);
                if (v == ONE) ones++;
                else if (v == UNKNOWN) anyUnknown = true;
            }
            if (anyUnknown) return UNKNOWN;
            return (ones % 2 == 1) ? ONE : ZERO;
        }
        case XNOR: {
            int ones = 0; bool anyUnknown = false;
            for (int id : g.inputs) {
                LogicVal v = getVal(id);
                if (v == ONE) ones++;
                else if (v == UNKNOWN) anyUnknown = true;
            }
            if (anyUnknown) return UNKNOWN;
            return (ones % 2 == 0) ? ONE : ZERO;
        }
        default:
            return UNKNOWN;
    }
}

void Circuit::applyInputs(const std::vector<LogicVal> &inputVals) {
    for (int i = 0; i < static_cast<int>(pis_.size()); ++i)
        gates_[pis_[i]].value = (i < static_cast<int>(inputVals.size())) ? inputVals[i] : ZERO;
}

void Circuit::simulate(const std::vector<LogicVal> &inputVals) {
    applyInputs(inputVals);
    for (auto &g : gates_) {
        if (g.ntype == PI) continue;
        g.value = evalGate(g);
    }
}

void Circuit::simulateWithFault(const std::vector<LogicVal> &inputVals, int faultGateId, int stuckAt) {
    applyInputs(inputVals);
    LogicVal forcedVal = (stuckAt == 0) ? ZERO : ONE;

    for (auto &g : gates_) {
        if (g.ntype == PI) {
            if (g.id == faultGateId) g.value = forcedVal;
            continue;
        }
        g.value = evalGate(g);
        if (g.id == faultGateId) g.value = forcedVal;
    }
}

std::vector<LogicVal> Circuit::getOutputs() const {
    std::vector<LogicVal> out;
    out.reserve(pos_.size());
    for (int idx : pos_) out.push_back(gates_[idx].value);
    return out;
}

} // namespace bist
