#include "../include/ScanChain.hpp"

namespace scan {

ScanChain::ScanChain(const std::vector<DFF> &dffs) {
    for (auto &dff : dffs) chain_.push_back(dff.qName);
    states_.assign(chain_.size(), ZERO);
}

void ScanChain::shiftIn(const std::vector<LogicVal> &pattern) {
    for (int i = 0; i < static_cast<int>(chain_.size()); ++i)
        states_[i] = i < static_cast<int>(pattern.size()) ? pattern[i] : ZERO;
}

std::vector<LogicVal> ScanChain::shiftOut(const Circuit &circuit) const {
    return circuit.getNextFFStates();
}

} // namespace scan
