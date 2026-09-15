#pragma once
#include "Types.hpp"
#include "Circuit.hpp"
#include <vector>
#include <string>

namespace scan {

class ScanChain {
public:
    explicit ScanChain(const std::vector<DFF> &dffs);

    void shiftIn(const std::vector<LogicVal> &pattern);
    std::vector<LogicVal> shiftOut(const Circuit &circuit) const;

    const std::vector<LogicVal> &states() const { return states_; }
    int length() const { return static_cast<int>(chain_.size()); }

private:
    std::vector<std::string> chain_;
    std::vector<LogicVal> states_;
};

} // namespace scan
