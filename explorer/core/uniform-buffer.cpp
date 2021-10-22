#include "core/uniform-buffer.h"

#define GL_GLEXT_PROTOTYPES
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif

#include <cstdio>
#include <cstring>

namespace ImVid {

UniformBuffer::UniformBuffer() { }

UniformBuffer::~UniformBuffer() {
    free();
}

bool UniformBuffer::create(uint32_t nbytes, Usage usage) {
    if (nbytes == 0) {
        fprintf(stderr, "Attempt to allocate uniform buffer with 0 bytes\n");
        return false;
    }

    free();

    m_nbytes = nbytes;

    glGenBuffers(1, &m_id);
    if (m_id == 0) {
        fprintf(stderr, "Failed to allocate uniform buffer\n");
        return false;
    }

    glBindBuffer(GL_UNIFORM_BUFFER, m_id);
    glBufferData(GL_UNIFORM_BUFFER, m_nbytes, nullptr,
                 usage == Static ? GL_STATIC_DRAW :
                 usage == Stream ? GL_STREAM_DRAW : GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    m_isAllocated = true;

    return true;
}

bool UniformBuffer::setData(const void * data, uint32_t nbytes, uint32_t offset) {
    if (m_isAllocated == false) {
        fprintf(stderr, "Attempt to setData of unallocated uniform buffer\n");
        return false;
    }
    if (data == nullptr) {
        fprintf(stderr, "Attempt to setData of uniform buffer with nullptr\n");
        return false;
    }
    if (nbytes == 0) {
        fprintf(stderr, "Attempt to setData of uniform buffer with 0 bytes\n");
        return false;
    }
    if (offset + nbytes > m_nbytes) {
        fprintf(stderr, "Attempt to setData with too much bytes (%d > %d)\n",
                offset + nbytes, m_nbytes);
        return false;
    }

    glBindBuffer(GL_UNIFORM_BUFFER, m_id);
    //glBufferData(GL_UNIFORM_BUFFER, nbytes, data, GL_STREAM_DRAW);
    glBufferSubData(GL_UNIFORM_BUFFER, offset, nbytes, data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    return true;
}

bool UniformBuffer::free() {
    if (m_isAllocated) {
        glDeleteBuffers(1, &m_id);
    }

    m_isAllocated = false;

    return true;
}

}
