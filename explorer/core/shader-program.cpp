#include "core/shader-program.h"

#include "core/frame-buffer.h"
#include "core/uniform-buffer.h"

#include "imgui/imgui.h"

#include <cstdio>

#if (defined(__APPLE__) && TARGET_OS_IOS) || (defined(__ANDROID__)) || (defined(__EMSCRIPTEN__))
static char g_GlslVersionString[32] = "#version 300 es\n";
#else
static char g_GlslVersionString[32] = "#version 150\n";
#endif

#define GL_GLEXT_PROTOTYPES
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif

#include <cmath>
#include <vector>

namespace {
	const char * kShaderLineRenderVertex = R"(
        in vec3 aPos;

        out vec4 aColor;

        void main()
        {
            gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
            aColor = vec4(0.0, 0.0, 0.0, aPos.z);
        })";

	const char * kShaderLineRenderFragment = R"(
        precision mediump float;

        in vec4 aColor;
        uniform vec4 color0;
        out vec4 FragColor;

        void main()
        {
            FragColor = vec4(color0.xyz, aColor.w);
        })";

    bool compile(uint32_t type, const char * source, uint32_t & shader) {
        shader = glCreateShader(type);
        const GLchar* vertex_shader_with_version[2] = { g_GlslVersionString, source };
        glShaderSource(shader, 2, vertex_shader_with_version, NULL);
        glCompileShader(shader);

        GLint compiled;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint maxLength = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
            std::vector<GLchar> infoLog(maxLength);
            glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);
            glDeleteShader(shader);

            fprintf(stderr, "Failed to compile shader (len = %d): %s\n", maxLength, infoLog.data());
            return false;
        }

        return true;
    }
}

namespace ImVid {
ShaderProgram::ShaderProgram() {}

ShaderProgram::~ShaderProgram() {
    free();
}

bool ShaderProgram::free() {
    glDeleteVertexArrays(1, &vaoHandle);
    glDeleteBuffers(1, &vboHandle);
    glDeleteBuffers(1, &eboHandle);

    glDeleteProgram(program);

    return true;
}

bool ShaderProgram::create(const char * sourceVertex, const char * sourceFragment) {
    if (free() == false) return false;

    if (::compile(GL_VERTEX_SHADER, sourceVertex, shaderVertex) == false) return false;
    if (::compile(GL_FRAGMENT_SHADER, sourceFragment, shaderFragment) == false) return false;

    m_isValid = false;

    program = glCreateProgram();
    glAttachShader(program, shaderVertex);
    glAttachShader(program, shaderFragment);

    glLinkProgram(program);

    GLint isLinked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, (int *)&isLinked);
    if (isLinked == GL_FALSE) {
        GLint maxLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<GLchar> infoLog(maxLength);
        glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
        fprintf(stderr, "Failed to compile GL program: %s\n", infoLog.data());

        glDeleteProgram(program);
        glDeleteShader(shaderVertex);
        glDeleteShader(shaderFragment);

        return false;
    }

    glDeleteShader(shaderVertex);
    glDeleteShader(shaderFragment);

    float vertices[] = {
        1.0f,  1.0f, 0.0f,  // top right
        1.0f, -1.0f, 0.0f,  // bottom right
        -1.0f, -1.0f, 0.0f,  // bottom left
        -1.0f,  1.0f, 0.0f   // top left
    };

    unsigned int indices[] = {  // note that we start from 0!
        0, 1, 3,  // first Triangle
        1, 2, 3   // second Triangle
    };

    glGenVertexArrays(1, &vaoHandle);
    glGenBuffers(1, &vboHandle);
    glGenBuffers(1, &eboHandle);
    glBindVertexArray(vaoHandle);

    glBindBuffer(GL_ARRAY_BUFFER, vboHandle);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboHandle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    uniformLocationFTime = glGetUniformLocation(program, "fTime");
    uniformLocationIResolution = glGetUniformLocation(program, "iResolution");
    uniformLocationColor0 = glGetUniformLocation(program, "color0");

    m_isValid = true;
    return true;
}

bool ShaderProgram::createLineRender() {
    return create(::kShaderLineRenderVertex, ::kShaderLineRenderFragment);
}

bool ShaderProgram::setData(const char * blockName, const uint32_t uboId) {
    auto index = glGetUniformBlockIndex(program, blockName);
    glBindBufferBase(GL_UNIFORM_BUFFER, index, uboId);

    return true;
}

bool ShaderProgram::setData(const char * blockName, const UniformBuffer & ubo) {
    auto index = glGetUniformBlockIndex(program, blockName);
    glBindBufferBase(GL_UNIFORM_BUFFER, index, ubo.getId());

    return true;
}

bool ShaderProgram::setTextureId(uint32_t texId) {
    this->texId = texId;

    return true;
}

bool ShaderProgram::render(const ImVid::FrameBuffer & fbo) const {
    fbo.bind();
    fbo.clear({ 0.0f, 0.0f, 0.0f, 0.0f }); // TODO : should be optional
    fbo.setViewport();
    use(fbo.getSizeX(), fbo.getSizeY());

    if (texId) {
        glBindTexture(GL_TEXTURE_2D, texId);
    }

    glBindBuffer(GL_ARRAY_BUFFER, vboHandle);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(0);
    glBindVertexArray(vaoHandle);
    //glDrawArrays(GL_TRIANGLES, 0, 6);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glDisableVertexAttribArray(0);

    fbo.unbind();

    return true;
}

bool ShaderProgram::renderLineStrip(
        const ImVid::FrameBuffer & fbo,
        const std::array<float, 4> & color,
        const std::vector<std::array<float, 2>> & points) {
    fbo.bind();
    fbo.setViewport();
    use(fbo.getSizeX(), fbo.getSizeY());

    int nPoints = points.size();
    std::vector<float> vertices(3*nPoints);
    for (int i = 0; i < nPoints; ++i) {
        vertices[3*i + 0] = points[i][0];
        vertices[3*i + 1] = points[i][1];
        vertices[3*i + 2] = 1.0f;
    }

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 3*points.size()*sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindVertexArray(vaoHandle);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glUniform4f(uniformLocationColor0, color[0], color[1], color[2], color[3]);
    glDrawArrays(GL_LINE_STRIP, 0, nPoints);
    glDisableVertexAttribArray(0);

    glDeleteBuffers(1, &vbo);

    fbo.unbind();

    return true;
}

bool ShaderProgram::renderLinesAsQuads(
        const ImVid::FrameBuffer & fbo,
        const std::array<float, 4> & color,
        const std::vector<std::array<float, 2>> & points,
        float thickness) {
    int nPoints = points.size();
    if (nPoints < 2) return false;

    fbo.bind();
    fbo.setViewport();
    use(fbo.getSizeX(), fbo.getSizeY());

    thickness /= 0.5f*fbo.getSizeX();

    int nSegments = nPoints/2;
    std::vector<float> vertices(24*nSegments, 0.0f);
    std::vector<uint32_t> indices(18*nSegments);
    for (int i = 0; i < nSegments; ++i) {
        int id0 = 2*i;
        int id1 = 2*i + 1;

        float feather = thickness;
        float dx = points[id1][0] - points[id0][0];
        float dy = points[id1][1] - points[id0][1];
        float id = std::sqrt(dx*dx + dy*dy);
        if (id > 0.0f) {
            id = 1.0f/id;
        } else {
            id = 0.0f;
        }

        float nx =  dy*id*thickness;
        float ny = -dx*id*thickness;
        float fx =  dy*id*(thickness + feather);
        float fy = -dx*id*(thickness + feather);

        vertices[24*i +  0] = points[id0][0] - nx; vertices[24*i +  1] = points[id0][1] - ny; vertices[24*i +  2] = color[3]; // 0
        vertices[24*i +  3] = points[id0][0] + nx; vertices[24*i +  4] = points[id0][1] + ny; vertices[24*i +  5] = color[3]; // 1
        vertices[24*i +  6] = points[id0][0] + fx; vertices[24*i +  7] = points[id0][1] + fy; vertices[24*i +  8] = 0.0f; // 2
        vertices[24*i +  9] = points[id1][0] + fx; vertices[24*i + 10] = points[id1][1] + fy; vertices[24*i + 11] = 0.0f; // 3
        vertices[24*i + 12] = points[id1][0] + nx; vertices[24*i + 13] = points[id1][1] + ny; vertices[24*i + 14] = color[3]; // 4
        vertices[24*i + 15] = points[id1][0] - nx; vertices[24*i + 16] = points[id1][1] - ny; vertices[24*i + 17] = color[3]; // 5
        vertices[24*i + 18] = points[id1][0] - fx; vertices[24*i + 19] = points[id1][1] - fy; vertices[24*i + 20] = 0.0f; // 6
        vertices[24*i + 21] = points[id0][0] - fx; vertices[24*i + 22] = points[id0][1] - fy; vertices[24*i + 23] = 0.0f; // 7

        indices[18*i +  0] = 8*i + 0; indices[18*i +  1] = 8*i + 1; indices[18*i +  2] = 8*i + 4;
        indices[18*i +  3] = 8*i + 0; indices[18*i +  4] = 8*i + 4; indices[18*i +  5] = 8*i + 5;
        indices[18*i +  6] = 8*i + 1; indices[18*i +  7] = 8*i + 2; indices[18*i +  8] = 8*i + 3;
        indices[18*i +  9] = 8*i + 1; indices[18*i + 10] = 8*i + 3; indices[18*i + 11] = 8*i + 4;
        indices[18*i + 12] = 8*i + 7; indices[18*i + 13] = 8*i + 0; indices[18*i + 14] = 8*i + 5;
        indices[18*i + 15] = 8*i + 7; indices[18*i + 16] = 8*i + 5; indices[18*i + 17] = 8*i + 6;
    }

    GLuint vbo;
    GLuint ebo;
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size()*sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(vaoHandle);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glUniform4f(uniformLocationColor0, color[0], color[1], color[2], color[3]);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    //glDrawArrays(GL_TRIANGLES, 0, nPoints);
    glDisableVertexAttribArray(0);

    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);

    fbo.unbind();

    return true;
}

bool ShaderProgram::renderPolygonFilled(
        const ImVid::FrameBuffer & fbo,
        const std::array<float, 4> & color,
        const std::vector<std::array<float, 2>> & points) {
    int nPoints = points.size();
    if (nPoints < 2) return false;

    fbo.bind();
    fbo.setViewport();
    use(fbo.getSizeX(), fbo.getSizeY());

    std::vector<float> vertices(3*(nPoints + 1), 0.0f);
    std::vector<uint32_t> indices((nPoints + 1));

    float sumx = 0.0f;
    float sumy = 0.0f;
    for (int ii = 0; ii < nPoints; ++ii) {
        int i = 1 + ii;
        vertices[3*i + 0] = points[ii][0];
        vertices[3*i + 1] = points[ii][1];
        vertices[3*i + 2] = color[3];
        indices[i] = i;

        sumx += points[ii][0];
        sumy += points[ii][1];
    }

    sumx /= nPoints;
    sumy /= nPoints;
    indices[0] = 0;
    vertices[0] = sumx;
    vertices[1] = sumy;
    vertices[2] = color[3];

    GLuint vbo;
    GLuint ebo;
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size()*sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(vaoHandle);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glUniform4f(uniformLocationColor0, color[0], color[1], color[2], color[3]);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glDrawElements(GL_TRIANGLE_FAN, indices.size(), GL_UNSIGNED_INT, 0);
    glDisableVertexAttribArray(0);

    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);

    fbo.unbind();

    return true;
}

bool ShaderProgram::use(int sizeX, int sizeY) const {
    if (m_isValid) {
        glUseProgram(program);

        glUniform1f(uniformLocationFTime, ImGui::GetTime());
        glUniform2f(uniformLocationIResolution, sizeX, sizeY);

        return true;
    }

    return false;
}
}
