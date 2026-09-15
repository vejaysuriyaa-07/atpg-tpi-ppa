#include "../includes/core/cread.hpp"
#include "../includes/utils.hpp"
#include "../includes/Simulator.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace logicsim {
CircuitLayout scanLayout(std::istream &input) {
    CircuitLayout layout{};
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream parser(line);
        int typeValue = 0;
        int nodeId = 0;
        if (!(parser >> typeValue >> nodeId)) {
            continue;
        }
        layout.maxId = std::max(layout.maxId, nodeId);
        ++layout.numNodes;
        if (typeValue == static_cast<int>(NodeType::PI)) {
            ++layout.nPI;
        } else if (typeValue == static_cast<int>(NodeType::PO)) {
            ++layout.nPO;
        }
    }
    return layout;
}

bool parseCircuit(std::istream &input, const CircuitLayout &layout, CircuitParseResult &result, std::string &error) {
    result = {};
    result.layout = layout;
    if (layout.maxId < 0) {
        error = "Circuit file is empty or malformed.";
        return false;
    }
    result.idToIndex.assign(static_cast<std::size_t>(layout.maxId + 1), -1);
    result.nodes.reserve(static_cast<std::size_t>(layout.numNodes));

    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        std::istringstream parser(line);
        int typeValue = 0;
        int nodeId = 0;
        if (!(parser >> typeValue >> nodeId)) {
            continue;
        }
        if (typeValue < static_cast<int>(NodeType::GATE) || typeValue > static_cast<int>(NodeType::PO)) {
            error = "Unknown node type for node " + std::to_string(nodeId) + " at line " + std::to_string(lineNumber) + ".";
            return false;
        }
        if (nodeId < 0 || nodeId >= static_cast<int>(result.idToIndex.size())) {
            error = "Node id " + std::to_string(nodeId) + " out of range at line " + std::to_string(lineNumber) + ".";
            return false;
        }
        if (result.idToIndex[static_cast<std::size_t>(nodeId)] != -1) {
            error = "Duplicate node id " + std::to_string(nodeId) + " at line " + std::to_string(lineNumber) + ".";
            return false;
        }

        ParsedNode parsed{};
        parsed.nodeId = nodeId;
        parsed.nodeType = static_cast<NodeType>(typeValue);

        switch (parsed.nodeType) {
            case NodeType::PI:
            case NodeType::PO:
            case NodeType::GATE: {
                int gateTypeValue = 0;
                int fanoutCount = 0;
                int faninCount = 0;
                if (!(parser >> gateTypeValue >> fanoutCount >> faninCount)) {
                    error = "Malformed entry for node " + std::to_string(nodeId) + " at line " + std::to_string(lineNumber) + ".";
                    return false;
                }
                parsed.gateType = static_cast<GateType>(gateTypeValue);
                parsed.fout = fanoutCount;
                parsed.fin = faninCount;
                break;
            }
            case NodeType::FB: {
                int gateTypeValue = 0;
                if (!(parser >> gateTypeValue)) {
                    error = "Malformed feedback entry for node " + std::to_string(nodeId) + " at line " + std::to_string(lineNumber) + ".";
                    return false;
                }
                parsed.fin = 1;
                parsed.fout = 1;
                parsed.gateType = static_cast<GateType>(gateTypeValue);
                break;
            }
        }

        parsed.faninIds.reserve(static_cast<std::size_t>(parsed.fin));
        for (int i = 0; i < parsed.fin; ++i) {
            int upstreamId = 0;
            if (!(parser >> upstreamId)) {
                error = "Missing fan-in definition for node " + std::to_string(nodeId) + " at line " + std::to_string(lineNumber) + ".";
                return false;
            }
            if (upstreamId < 0 || upstreamId >= layout.maxId + 1) {
                error = "Fan-in node " + std::to_string(upstreamId) + " out of range for node " + std::to_string(nodeId) + ".";
                return false;
            }
            parsed.faninIds.push_back(upstreamId);
        }

        result.idToIndex[static_cast<std::size_t>(nodeId)] = static_cast<int>(result.nodes.size());
        result.nodes.push_back(std::move(parsed));
    }

    if (static_cast<int>(result.nodes.size()) != layout.numNodes) {
        error = "Node count mismatch. Expected " + std::to_string(layout.numNodes) + " but parsed " + std::to_string(result.nodes.size()) + ".";
        return false;
    }

    return true;
}

bool materializeCircuit(const CircuitParseResult &parsed, std::vector<Node> &nodes, std::vector<Node *> &primaryInputs, std::vector<Node *> &primaryOutputs, std::string &error) {
    int ni = 0;
    int no = 0;

    if (nodes.size() != parsed.nodes.size()) {
        error = "Allocated node count does not match parsed node count.";
        return false;
    }

    for (std::size_t index = 0; index < parsed.nodes.size(); ++index) {
        const ParsedNode &src = parsed.nodes[index];
        Node &node = nodes[index];

        node.setNum(static_cast<unsigned>(src.nodeId));
        node.setNtype(src.nodeType);
        node.setType(src.gateType);
        node.setFin(static_cast<unsigned>(src.fin));
        node.setFout(static_cast<unsigned>(src.fout));

        if (src.nodeType == NodeType::PI) {
            if (ni >= static_cast<int>(primaryInputs.size())) {
                error = "Primary input count mismatch.";
                return false;
            }
            primaryInputs[static_cast<std::size_t>(ni++)] = &node;
        } else if (src.nodeType == NodeType::PO) {
            if (no >= static_cast<int>(primaryOutputs.size())) {
                error = "Primary output count mismatch.";
                return false;
            }
            primaryOutputs[static_cast<std::size_t>(no++)] = &node;
        }

        if (node.getFin() > 0) {
            Node **unodeStorage = new Node *[node.getFin()];
            for (unsigned faninIndex = 0; faninIndex < node.getFin(); ++faninIndex) {
                const int upstreamId = src.faninIds[faninIndex];
                if (upstreamId < 0 || upstreamId >= static_cast<int>(parsed.idToIndex.size())) {
                    delete[] unodeStorage;
                    error = "Fan-in node " + std::to_string(upstreamId) + " out of range for node " + std::to_string(src.nodeId) + ".";
                    return false;
                }
                const int upstreamIndex = parsed.idToIndex[static_cast<std::size_t>(upstreamId)];
                if (upstreamIndex < 0) {
                    delete[] unodeStorage;
                    error = "Undefined fan-in node " + std::to_string(upstreamId) + " referenced by node " + std::to_string(src.nodeId) + ".";
                    return false;
                }
                unodeStorage[faninIndex] = &nodes[static_cast<std::size_t>(upstreamIndex)];
            }
            node.setUnodes(unodeStorage);
        } else {
            node.setUnodes(nullptr);
        }

        if (node.getFout() > 0) {
            Node **dnodeStorage = new Node *[node.getFout()];
            std::fill_n(dnodeStorage, node.getFout(), nullptr);
            node.setDnodes(dnodeStorage);
        } else {
            node.setDnodes(nullptr);
        }
    }

    if (ni != static_cast<int>(primaryInputs.size())) {
        error = "Primary input count mismatch.";
        return false;
    }
    if (no != static_cast<int>(primaryOutputs.size())) {
        error = "Primary output count mismatch.";
        return false;
    }

    return true;
}

void linkFanouts(std::vector<Node> &nodes) {
    for (Node &current : nodes) {
        for (unsigned faninIndex = 0; faninIndex < current.getFin(); ++faninIndex) {
            Node *upstream = current.getUnodes()[faninIndex];
            if (upstream == nullptr) {
                continue;
            }
            unsigned fanoutIndex = 0;
            while (fanoutIndex < upstream->getFout() && upstream->getDnodes()[fanoutIndex] != nullptr) {
                ++fanoutIndex;
            }
            if (fanoutIndex < upstream->getFout()) {
                upstream->getDnodes()[fanoutIndex] = &current;
            }
        }
    }
}

int cread_impl(Simulator &simulator) {
    std::string path = trimCopy(simulator.getCommandArgs());
    if (path.empty()) {
        std::cerr << "READ requires a circuit file path.\n";
        return 0;
    }

    std::ifstream circuitFile(path);
    if (!circuitFile.is_open()) {
        std::cerr << "File does not exist!\n";
        return 0;
    }

    simulator.setInpName(path);

    if (simulator.getGlobalState() >= State::CKTLD) {
        simulator.clear();
    }

    CircuitLayout layout = scanLayout(circuitFile);
    if (layout.numNodes == 0) {
        std::cerr << "Circuit file is empty or malformed.\n";
        return 0;
    }

    simulator.setNumNodes(layout.numNodes);
    simulator.setNPI(layout.nPI);
    simulator.setNPO(layout.nPO);

    circuitFile.clear();
    circuitFile.seekg(0, std::ios::beg);

    CircuitParseResult parsed;
    std::string error;
    if (!parseCircuit(circuitFile, layout, parsed, error)) {
        std::cerr << error << '\n';
        return 0;
    }

    simulator.allocate();

    if (!materializeCircuit(parsed, simulator.getNodes(), simulator.getPrimaryInputs(), simulator.getPrimaryOutputs(), error)) {
        std::cerr << error << '\n';
        return 0;
    }

    linkFanouts(simulator.getNodes());

    simulator.setGlobalState(State::CKTLD);
    std::cout << "==> OK";
    return 1;
}
} // namespace logicsim
