#pragma once
#include <cstdint>
#include <vector>

namespace bist {

class MISR {
public:
    explicit MISR(int width);

    void compress(const std::vector<int> &outputs);
    uint64_t signature() const { return state_; }
    void reset() { state_ = 0; }
    int width() const { return width_; }

private:
    int width_;
    uint64_t state_;
    uint64_t poly_;
    uint64_t mask_;
};

} // namespace bist
