#include "rle.hpp"
#include <cstddef>

std::vector<RLEPair> rle_compress(const std::vector<uint32_t>& input) {
    std::vector<RLEPair> out;
    if (input.empty()) return out;

    uint32_t current = input[0];
    uint32_t count = 1;

    for (std::size_t i = 1; i < input.size(); i++) {
        if (input[i] == current) {
            count++;
        } else {
            out.push_back({current, count});
            current = input[i];
            count = 1;
        }
    }

    out.push_back({current, count});
    return out;
}

std::vector<uint32_t> rle_decompress_cpu(const std::vector<RLEPair>& input) {
    std::size_t total_size = 0;
    for (const auto& p : input) {
        total_size += p.count;
    }

    std::vector<uint32_t> out(total_size);
    std::size_t pos = 0;

    for (const auto& p : input) {
        for (uint32_t i = 0; i < p.count; i++) {
            out[pos++] = p.value;
        }
    }

    return out;
}
