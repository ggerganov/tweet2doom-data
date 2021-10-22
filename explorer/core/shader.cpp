#include "core/shader.h"

#include "core/utils.h"
#include "core/canvas.h"
#include "core/image.h"
#include "core/frame-buffer.h"
#include "core/shader-program.h"

#include "imgui/imgui.h"

namespace {
	const char * kDefaultShaderVertex = R"(
        in vec3 aPos;

        void main()
        {
            gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
        })";

	const char * kDefaultShaderFragment = R"(
        precision mediump float;

        uniform float fTime;
        uniform vec2 iResolution;
        out vec4 FragColor;

        float random(vec2 p)
        {
            vec2 K1 = vec2(23.14069263277926, 2.665144142690225);
            return fract(cos(dot(p,K1))*12345.6789);
        }

        void main()
        {
            vec2 FragCoord = gl_FragCoord.xy/iResolution;
            float r = random(FragCoord*fTime);
            FragColor = vec4(r, r, r, 0.2);
        })";
}

namespace ImVid {

struct Shader::Data {
    Content content;

    ImVid::FrameBuffer fbo;
    ImVid::ShaderProgram shaderProgram;
};

Shader::Shader() : m_data(new Data()) {
    m_params.canvas = std::make_shared<Canvas>();
}

Shader::~Shader() {}

bool Shader::render() {
    ImGui::Image((void *)(intptr_t) m_data->fbo.getIdTex(), m_params.canvas->getSize<ImVec2>(),
                 ImVec2(m_params.canvas->viewCenterX - 0.5f*m_params.canvas->viewSizeX,
                        m_params.canvas->viewCenterY - 0.5f*m_params.canvas->viewSizeY),
                 ImVec2(m_params.canvas->viewCenterX + 0.5f*m_params.canvas->viewSizeX,
                        m_params.canvas->viewCenterY + 0.5f*m_params.canvas->viewSizeY),
                 ImColor(255, 255, 255, 255), ImColor(255, 255, 255, 0));

    return true;
}

bool Shader::update() {
    bool result = true;

    if (m_params.updateContent) {
        if (const auto & cb = m_params.cbGetContent) {
            m_data->content = cb();

            if (m_data->content.shaderFragment) {
                result &= m_data->shaderProgram.create(::kDefaultShaderVertex, m_data->content.shaderFragment);
            } else {
                result &= m_data->shaderProgram.create(::kDefaultShaderVertex, ::kDefaultShaderFragment);
            }
        } else {
            result = false;
        }

        if (result) {
            result &= m_data->fbo.create(m_data->content.sizeX, m_data->content.sizeY);
            result &= m_data->shaderProgram.setTextureId(m_data->content.texId);
            for (const auto & d : m_data->content.dataHost) {
                result &= m_data->shaderProgram.setData(d.first, d.second);
            }
        }
    }

    m_data->shaderProgram.render(m_data->fbo);

    m_params.updateContent = false;

    return result;
}

}
