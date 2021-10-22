#include "core/image.h"

#include "core/utils.h"
#include "core/canvas.h"

#include "imgui/imgui.h"

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif

#include <cmath>
#include <vector>

#define GL_CLAMP_TO_EDGE 0x812F

namespace ImVid {

struct Image::Data {
    int nDraws = 0;

    std::vector<uint8_t> texture;

    Content content;

    GLuint texId = 0;

    bool generateTexture(const Parameters & params) {
        if (texId > 0) {
            glDeleteTextures(1, &texId);
        }

        GLint last_texture;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
        glGenTextures(1, &texId);
        glBindTexture(GL_TEXTURE_2D, texId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (params.texture.linearInterpolation) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

        const uint8_t * src = (const uint8_t *) content.pixels;
        if (content.store) {
            texture.resize(3*content.nx*content.ny);
            std::copy(src, src + texture.size(), texture.data());
            src = texture.data();
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, content.nx, content.ny, 0, GL_RGB, GL_UNSIGNED_BYTE, src);

        // restore last texture
        glBindTexture(GL_TEXTURE_2D, last_texture);

        ++nDraws;

        return true;
    }
};

Image::Image() : m_data(new Data()) {
    m_params.canvas = std::make_shared<Canvas>();
}

Image::~Image() {}

bool Image::render() {
    const auto & canvas = m_params.canvas;
    const auto & content = m_data->content;

    m_params.canvas->setPos(ImGui::GetCursorScreenPos());

    if (content.pixels != nullptr) {
        ImGui::SetCursorScreenPos(canvas->getPos<ImVec2>());
        ImGui::Image((void *)(intptr_t) m_data->texId, m_params.canvas->getSize<ImVec2>(),
                     ImVec2(m_params.canvas->viewCenterX - 0.5f*m_params.canvas->viewSizeX,
                            m_params.canvas->viewCenterY - 0.5f*m_params.canvas->viewSizeY),
                     ImVec2(m_params.canvas->viewCenterX + 0.5f*m_params.canvas->viewSizeX,
                            m_params.canvas->viewCenterY + 0.5f*m_params.canvas->viewSizeY),
                     ImColor(255, 255, 255, 255), ImColor(255, 255, 255, 0));
    }

    return true;
}

bool Image::update() {
    m_data->nDraws = 0;

    bool result = true;
    bool regenerate = false;

    if (m_params.updateContent) {
        if (const auto & cb = m_params.cbGetContent) {
            m_data->content = cb();
            regenerate = true;
        } else {
            result = false;
        }
    }

    regenerate &= m_data->content.nx*m_data->content.ny > 0;

    if (regenerate) {
        result &= m_data->generateTexture(m_params);
    }

    m_params.updateContent = false;

    return result;
}

uint32_t Image::getTextureId() const {
    return m_data->texId;
}

int Image::getDraws() const {
    return m_data->nDraws;
}

}
