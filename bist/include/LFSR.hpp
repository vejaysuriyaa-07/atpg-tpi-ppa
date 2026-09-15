#pragma once
#include <cstdint>
#include <vector>

namespace bist {

class LFSR {
public:
    explicit LFSR(int width, uint32_t seed = 0xACE1u);

    uint64_t next();
    std::vector<int> generatePattern(int numBits);
    int width() const { return width_; }

private:
    int width_;
    uint64_t state_;
    uint64_t poly_;

    static uint64_t primitivePolynomial(int width);
};

} // namespace bist
