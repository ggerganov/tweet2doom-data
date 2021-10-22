#include "core/utils.h"

#include <chrono>

namespace ImVid {

int64_t t_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count(); // duh ..
}

}
