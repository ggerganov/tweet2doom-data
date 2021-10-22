#pragma once

#include <cstdint>

namespace ImVid {
    int64_t t_us();

    template <class T> T toVec4(const float * arr) { return T { arr[0], arr[1], arr[2], arr[3] }; }
    template <class T> T toVec4(const float * arr, float a) { return T { arr[0], arr[1], arr[2], a }; }

    template <class T>
    void cpy4(T src [], T dst []) {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
    }
}
