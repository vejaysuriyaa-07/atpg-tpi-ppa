#include "../include/LFSR.hpp"
#include <stdexcept>

namespace bist {

uint64_t LFSR::primitivePolynomial(int width) {
    switch (width) {
        case 4:  return 0x9;
        case 8:  return 0xB8;
        case 16: return 0xB400;
        case 20: return 0x90000;
        case 24: return 0xE10000;
        case 32: return 0xB4BCD35C;
        default:
            if (width <= 4)  return 0x9;
            if (width <= 8)  return 0xB8;
            if (width <= 16) return 0xB400;
            if (width <= 32) return 0xB4BCD35C;
            return 0xB4BCD35C;
    }
}

LFSR::LFSR(int width, uint32_t seed) : width_(width) {
    poly_ = primitivePolynomial(width);
    uint64_t mask = (width_ < 64) ? ((uint64_t(1) << width_) - 1) : ~uint64_t(0);
    state_ = seed & mask;
    if (state_ == 0) state_ = 1;
}

uint64_t LFSR::next() {
    uint64_t mask = (width_ < 64) ? ((uint64_t(1) << width_) - 1) : ~uint64_t(0);
    uint64_t bit = __builtin_parityll(state_ & poly_);
    state_ = ((state_ >> 1) | (bit << (width_ - 1))) & mask;
    return state_;
}

std::vector<int> LFSR::generatePattern(int numBits) {
    std::vector<int> bits;
    bits.reserve(numBits);
    while (static_cast<int>(bits.size()) < numBits) {
        uint64_t val = next();
        for (int i = 0; i < width_ && static_cast<int>(bits.size()) < numBits; ++i) {
            bits.push_back((val >> i) & 1);
        }
    }
    return bits;
}

} // namespace bist
