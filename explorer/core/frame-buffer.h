#pragma once

#include <cstdint>
#include <array>

namespace ImVid {

struct FrameBuffer {
public:
    FrameBuffer();
    ~FrameBuffer();

    bool create(int sizeX, int sizeY);
    bool free();
    bool isAllocated() const { return m_isAllocated; }

    bool bind() const;
    bool clear(const std::array<float, 4> & color = { { 0.0f, 0.0f, 0.0f, 0.0f } }) const;
    bool setViewport() const;
    bool unbind() const;

    uint32_t getId() const { return m_id; }
    uint32_t getIdTex() const { return m_idTex; }
    uint32_t getIdDepth() const { return m_idDepth; }

    int32_t getSizeX() const { return m_sizeX; }
    int32_t getSizeY() const { return m_sizeY; }

private:
    bool m_isAllocated = false;

    uint32_t m_id = 0;
    uint32_t m_idTex = 0;
    uint32_t m_idDepth = 0;

    int32_t m_sizeX = 512;
    int32_t m_sizeY = 512;
};

}
