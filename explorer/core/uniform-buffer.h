#pragma once

#include <cstdint>

namespace ImVid {

struct UniformBuffer {
public:
    enum Usage {
        Static,  // modified once and used many times
        Stream,  // modified once and used at most a few times
        Dynamic, // modified repeatedly and used many times
    };

    UniformBuffer();
    ~UniformBuffer();

    bool create(uint32_t nbytes, Usage usage = Dynamic);
    bool setData(const void * data, uint32_t nbytes, uint32_t offset = 0);
    bool free();
    bool isAllocated() const { return m_isAllocated; }

    uint32_t getId() const { return m_id; }
    int32_t getSize() const { return m_nbytes; }

private:
    bool m_isAllocated = false;

    uint32_t m_id = 0;
    uint32_t m_nbytes = 0;
};

}
