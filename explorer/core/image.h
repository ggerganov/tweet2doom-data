#pragma once

#include <memory>
#include <functional>

namespace ImVid {
    struct Canvas;

    class Image {
        public:
            struct Parameters;

            struct Content {
                // the Image object makes a copy of the pixel data
                bool store = false;

                // size of the image
                int nx = 0;
                int ny = 0;

                // pixel data
                const void * pixels = nullptr;
            };

            using CBGetContent = std::function<Content(void)>;

            struct Parameters {
                bool updateContent = true;

                struct Texture {
                    bool linearInterpolation = false;
                };
                Texture texture;

                std::shared_ptr<Canvas> canvas;

                CBGetContent cbGetContent = nullptr;
            };

            Image();
            ~Image();

            Parameters & getParameters() { return m_params; }

            bool render();
            bool update();

            uint32_t getTextureId() const;

            // stats
            int getDraws() const;

        private:
            struct Data;
            std::unique_ptr<Data> m_data;

            Parameters m_params;
    };

}
