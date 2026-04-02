#pragma once
#include <cstddef>
#include <vector>

typedef unsigned char byte;

namespace screen_recorder {

struct video_data {
    std::vector<byte> compressed_data;
    size_t size;

    void Reset() {
        compressed_data.clear();
        size = 0;
    }

    video_data() {
        size = 0;
    }

    video_data(const video_data& other) {
        compressed_data.assign(other.compressed_data.begin(), other.compressed_data.end());
        size = other.size;
    }

    video_data& operator=(const video_data& other) {
        if (this != &other) {
            compressed_data.assign(other.compressed_data.begin(), other.compressed_data.end());
            size = other.size;
        }

        return *this;
    }
};
}  // namespace screen_recorder