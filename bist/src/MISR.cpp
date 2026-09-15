#include "../include/MISR.hpp"
#include <stdexcept>

namespace bist {

static uint64_t misrPoly(int width) {
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
            return 0xB4BCD35C;
    }
}

MISR::MISR(int width) : width_(width), state_(0) {
    poly_ = misrPoly(width);
    mask_ = (width < 64) ? ((uint64_t(1) << width) - 1) : ~uint64_t(0);
}

void MISR::compress(const std::vector<int> &outputs) {
    uint64_t bit = __builtin_parityll(state_ & poly_);
    state_ = ((state_ >> 1) | (bit << (width_ - 1))) & mask_;

    for (int i = 0; i < static_cast<int>(outputs.size()); ++i) {
        int pos = i % width_;
        if (outputs[i]) {
            state_ ^= (uint64_t(1) << pos);
        }
    }
    state_ &= mask_;
}

} // namespace bist
