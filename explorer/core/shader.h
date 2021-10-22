#pragma once

#include <map>
#include <memory>
#include <functional>

namespace ImVid {
struct Canvas;

class Shader {
public:
    struct Parameters;

    struct Content {
        // nullptr to use default shader code
        const char * shaderFragment = nullptr;

        // input texture for the shader, 0 for no input
        uint32_t texId = 0;

        // frame buffer size
        int sizeX = 128;
        int sizeY = 128;

        // input uniform buffers
        std::map<const char *, uint32_t> dataHost;
    };

    using CBGetContent = std::function<Content(void)>;

    struct Parameters {
        bool updateContent = true;

        std::shared_ptr<Canvas> canvas;

        CBGetContent cbGetContent = nullptr;
    };

    Shader();
    ~Shader();

    Parameters & getParameters() { return m_params; }

    bool render();
    bool update();

private:
    struct Data;
    std::unique_ptr<Data> m_data;

    Parameters m_params;
};

}
