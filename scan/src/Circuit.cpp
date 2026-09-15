#include "../include/Circuit.hpp"
#include <algorithm>
#include <fstream>
#include <queue>
#include <set>
#include <sstream>

namespace scan {

static GateType parseGateType(const std::string &s) {
    if (s == "AND")  return G_AND;
    if (s == "OR")   return G_OR;
    if (s == "NAND") return G_NAND;
    if (s == "NOR")  return G_NOR;
    if (s == "NOT")  return G_NOT;
    if (s == "BUFF" || s == "BUF") return G_BUFF;
    if (s == "XOR")  return G_XOR;
    if (s == "XNOR") return G_XNOR;
    return G_BUFF;
}

static std::string trim(const std::string &s) {
    auto a = s.find_first_not_of(" \t\r\n");
    auto b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

static std::vector<std::string> splitComma(const std::string &s) {
    std::vector<std::string> parts;
    std::istringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ','))
        parts.push_back(trim(tok));
    return parts;
}

bool Circuit::loadFromFile(const std::string &path, std::string &error) {
    std::ifstream f(path);
    if (!f.is_open()) { error = "Cannot open: " + path; return false; }

    std::string base = path;
    auto sl = base.rfind('/');
    if (sl != std::string::npos) base = base.substr(sl + 1);
    auto dt = base.rfind('.');
    if (dt != std::string::npos) base = base.substr(0, dt);
    name_ = base;

    gates_.clear(); dffs_.clear(); pis_.clear(); pos_.clear(); nameToIdx_.clear();

    std::set<std::string> dffQNames;
    std::vector<std::tuple<std::string, std::string, std::string>> pendingGates;

    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (line.substr(0, 6) == "INPUT(") {
            std::string nm = trim(line.substr(6, line.size() - 7));
            pis_.push_back(nm);
            Gate g; g.name = nm; g.gtype = G_SOURCE; g.value = UNKNOWN; g.level = 0;
            nameToIdx_[nm] = static_cast<int>(gates_.size());
            gates_.push_back(g);
        } else if (line.substr(0, 7) == "OUTPUT(") {
            std::string nm = trim(line.substr(7, line.size() - 8));
            pos_.push_back(nm);
        } else {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string lhs = trim(line.substr(0, eq));
            std::string rhs = trim(line.substr(eq + 1));

            auto lp = rhs.find('(');
            auto rp = rhs.rfind(')');
            if (lp == std::string::npos || rp == std::string::npos) continue;

            std::string typeStr = trim(rhs.substr(0, lp));
            std::string argsStr = trim(rhs.substr(lp + 1, rp - lp - 1));

            if (typeStr == "DFF") {
                std::string dInput = trim(argsStr);
                DFF dff; dff.qName = lhs; dff.dName = dInput;
                dffs_.push_back(dff);
                dffQNames.insert(lhs);
                Gate g; g.name = lhs; g.gtype = G_SOURCE; g.value = UNKNOWN; g.level = 0;
                nameToIdx_[lhs] = static_cast<int>(gates_.size());
                gates_.push_back(g);
            } else {
                pendingGates.emplace_back(lhs, typeStr, argsStr);
            }
        }
    }

    for (auto &tup : pendingGates) {
        Gate g;
        g.name = std::get<0>(tup);
        g.gtype = parseGateType(std::get<1>(tup));
        g.inputs = splitComma(std::get<2>(tup));
        g.value = UNKNOWN;
        g.level = -1;
        nameToIdx_[g.name] = static_cast<int>(gates_.size());
        gates_.push_back(g);
    }

    levelize();
    buildFaultList();
    return true;
}

void Circuit::levelize() {
    std::unordered_map<std::string, int> indegree;
    for (auto &g : gates_) indegree[g.name] = 0;
    for (auto &g : gates_)
        for (auto &in : g.inputs)
            if (nameToIdx_.count(in))
                indegree[g.name]++;

    std::unordered_map<std::string, std::vector<std::string>> fanout;
    for (auto &g : gates_)
        for (auto &in : g.inputs)
            fanout[in].push_back(g.name);

    std::queue<std::string> q;
    for (auto &g : gates_) {
        if (g.inputs.empty()) {
            g.level = 0;
            q.push(g.name);
        }
    }

    while (!q.empty()) {
        std::string cur = q.front(); q.pop();
        int curLevel = gates_[nameToIdx_[cur]].level;
        for (auto &outName : fanout[cur]) {
            auto &og = gates_[nameToIdx_[outName]];
            og.level = std::max(og.level, curLevel + 1);
            if (--indegree[outName] == 0) q.push(outName);
        }
    }

    std::stable_sort(gates_.begin(), gates_.end(), [](const Gate &a, const Gate &b) {
        return a.level < b.level;
    });

    nameToIdx_.clear();
    for (int i = 0; i < static_cast<int>(gates_.size()); ++i)
        nameToIdx_[gates_[i].name] = i;
}

void Circuit::buildFaultList() {
    faults_.clear();
    for (auto &g : gates_) {
        faults_.push_back({g.name, 0});
        faults_.push_back({g.name, 1});
    }
}

LogicVal Circuit::evalGate(const Gate &g) const {
    auto get = [&](const std::string &nm) -> LogicVal {
        auto it = nameToIdx_.find(nm);
        return it == nameToIdx_.end() ? UNKNOWN : gates_[it->second].value;
    };

    switch (g.gtype) {
        case G_SOURCE:
        case G_BUFF: return get(g.inputs[0]);
        case G_NOT: {
            LogicVal v = get(g.inputs[0]);
            return v == ONE ? ZERO : v == ZERO ? ONE : UNKNOWN;
        }
        case G_AND: {
            bool anyZ = false, anyU = false;
            for (auto &in : g.inputs) { LogicVal v = get(in); if (v==ZERO) anyZ=true; else if (v==UNKNOWN) anyU=true; }
            return anyZ ? ZERO : anyU ? UNKNOWN : ONE;
        }
        case G_NAND: {
            bool anyZ = false, anyU = false;
            for (auto &in : g.inputs) { LogicVal v = get(in); if (v==ZERO) anyZ=true; else if (v==UNKNOWN) anyU=true; }
            return anyZ ? ONE : anyU ? UNKNOWN : ZERO;
        }
        case G_OR: {
            bool anyO = false, anyU = false;
            for (auto &in : g.inputs) { LogicVal v = get(in); if (v==ONE) anyO=true; else if (v==UNKNOWN) anyU=true; }
            return anyO ? ONE : anyU ? UNKNOWN : ZERO;
        }
        case G_NOR: {
            bool anyO = false, anyU = false;
            for (auto &in : g.inputs) { LogicVal v = get(in); if (v==ONE) anyO=true; else if (v==UNKNOWN) anyU=true; }
            return anyO ? ZERO : anyU ? UNKNOWN : ONE;
        }
        case G_XOR: {
            int ones = 0; bool anyU = false;
            for (auto &in : g.inputs) { LogicVal v = get(in); if (v==ONE) ones++; else if (v==UNKNOWN) anyU=true; }
            return anyU ? UNKNOWN : (ones % 2 ? ONE : ZERO);
        }
        case G_XNOR: {
            int ones = 0; bool anyU = false;
            for (auto &in : g.inputs) { LogicVal v = get(in); if (v==ONE) ones++; else if (v==UNKNOWN) anyU=true; }
            return anyU ? UNKNOWN : (ones % 2 ? ZERO : ONE);
        }
        default: return UNKNOWN;
    }
}

void Circuit::setSources(const std::vector<LogicVal> &piVals, const std::vector<LogicVal> &ffStates) {
    for (int i = 0; i < static_cast<int>(pis_.size()); ++i)
        gates_[nameToIdx_[pis_[i]]].value = i < static_cast<int>(piVals.size()) ? piVals[i] : ZERO;
    for (int i = 0; i < static_cast<int>(dffs_.size()); ++i)
        gates_[nameToIdx_[dffs_[i].qName]].value = i < static_cast<int>(ffStates.size()) ? ffStates[i] : ZERO;
}

void Circuit::simulate(const std::vector<LogicVal> &piVals, const std::vector<LogicVal> &ffStates) {
    setSources(piVals, ffStates);
    for (auto &g : gates_) {
        if (g.inputs.empty()) continue;
        g.value = evalGate(g);
    }
}

void Circuit::simulateWithFault(const std::vector<LogicVal> &piVals, const std::vector<LogicVal> &ffStates, const Fault &f) {
    LogicVal forced = f.stuckAt ? ONE : ZERO;
    setSources(piVals, ffStates);
    if (nameToIdx_.count(f.gateName) && gates_[nameToIdx_[f.gateName]].inputs.empty())
        gates_[nameToIdx_[f.gateName]].value = forced;
    for (auto &g : gates_) {
        if (g.inputs.empty()) continue;
        g.value = evalGate(g);
        if (g.name == f.gateName) g.value = forced;
    }
}

std::vector<LogicVal> Circuit::getOutputs() const {
    std::vector<LogicVal> out;
    for (auto &nm : pos_) {
        auto it = nameToIdx_.find(nm);
        out.push_back(it != nameToIdx_.end() ? gates_[it->second].value : UNKNOWN);
    }
    return out;
}

std::vector<LogicVal> Circuit::getNextFFStates() const {
    std::vector<LogicVal> next;
    for (auto &dff : dffs_) {
        auto it = nameToIdx_.find(dff.dName);
        next.push_back(it != nameToIdx_.end() ? gates_[it->second].value : UNKNOWN);
    }
    return next;
}

} // namespace scan
