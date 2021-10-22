#include "core/frame-buffer.h"

#define GL_GLEXT_PROTOTYPES
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif

#include <cassert>
#include <cstdio>

namespace ImVid {

FrameBuffer::FrameBuffer() { }

FrameBuffer::~FrameBuffer() {
    free();
}

bool FrameBuffer::create(int sizeX, int sizeY) {
    free();

    GLint last_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);

    glGenFramebuffers(1, &m_id);
    if (m_id == 0) {
        fprintf(stderr, "Failed to generate frame buffer\n");
        return false;
    }
    glGenTextures(1, &m_idTex);
    if (m_idTex == 0) {
        fprintf(stderr, "Failed to generate texture\n");
        return false;
    }
    //glGenRenderbuffers(1, &idDepth);

    m_sizeX = sizeX;
    m_sizeY = sizeY;

    glBindFramebuffer(GL_FRAMEBUFFER, m_id);

    glBindTexture(GL_TEXTURE_2D, m_idTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_sizeX, m_sizeY, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_idTex, 0);

    //glBindRenderbuffer(GL_RENDERBUFFER, idDepth);
    //glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, sizeX, sizeY);
    ////glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, idDepth);
    //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, idDepth);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Failed to create frame buffer!\n");
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, last_texture);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_isAllocated = true;

    return true;
}

bool FrameBuffer::free() {
    if (m_isAllocated) {
        assert(m_id > 0);
        assert(m_idTex > 0);

        //glDeleteRenderbuffers(1, &idDepth);
        glDeleteTextures(1, &m_idTex);
        glDeleteFramebuffers(1, &m_id);
    }

    m_isAllocated = false;

    return true;
}

bool FrameBuffer::bind() const {
    if (!m_isAllocated) {
        fprintf(stderr, "Attempt to bind unallocated frame buffer\n");
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_id);

    return true;
}

bool FrameBuffer::setViewport() const {
    glViewport(0, 0, m_sizeX, m_sizeY);

    return true;
}

bool FrameBuffer::clear(const std::array<float, 4> & color) const {
    glClearColor(color[0], color[1], color[2], color[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    return true;
}

bool FrameBuffer::unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

}
