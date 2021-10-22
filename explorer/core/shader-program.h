#pragma once

#include <cstdint>
#include <array>
#include <vector>

namespace ImVid {
struct UniformBuffer;
struct FrameBuffer;

struct ShaderProgram {
public:
    ShaderProgram();
    ~ShaderProgram();

    bool free();
    bool create(const char * sourceVertex, const char * sourceFragment);
    bool createLineRender();
    bool setData(const char * blockName, const uint32_t uboId);
    bool setData(const char * blockName, const UniformBuffer & ubo);
    bool setTextureId(uint32_t texId);

    bool render(const ImVid::FrameBuffer & fbo) const;

    bool renderLineStrip(
            const ImVid::FrameBuffer & fbo,
            const std::array<float, 4> & color,
            const std::vector<std::array<float, 2>> & points);

    bool renderLinesAsQuads(
            const ImVid::FrameBuffer & fbo,
            const std::array<float, 4> & color,
            const std::vector<std::array<float, 2>> & points,
            float thickness = 1.0f);

    bool renderPolygonFilled(
            const ImVid::FrameBuffer & fbo,
            const std::array<float, 4> & color,
            const std::vector<std::array<float, 2>> & points);

    bool use(int sizeX, int sizeY) const;

    uint32_t getVaoHandle() const { return vaoHandle; }

    bool isValid() const { return m_isValid; }

private:
    bool m_isValid = false;

    uint32_t texId = 0;

    uint32_t uniformLocationFTime = 0;
    uint32_t uniformLocationIResolution = 0;
    uint32_t uniformLocationColor0 = 0;

    uint32_t shaderVertex = 0;
    uint32_t shaderFragment = 0;
    uint32_t program = 0;

    uint32_t vaoHandle = 0;
    uint32_t vboHandle = 0;
    uint32_t eboHandle = 0;
};

}
