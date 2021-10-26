#include "core/assets.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb/stb_image.h"

#include <GLES2/gl2.h>
#if defined(__EMSCRIPTEN__)
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <GLES2/gl2ext.h>
#endif

#include <map>
#include <string>

namespace {
    using Filename = const char *;

    const std::map<::ImVid::Assets::Id, Filename> kFilenameAsset = {
        { ::ImVid::Assets::ICON_T2D_BIG,        "logo-big.png" },
        { ::ImVid::Assets::ICON_T2D_SMALL,      "logo-small.png" },
        { ::ImVid::Assets::ICON_T2D_SMALL_BLUR, "logo-small-blur.png" },
    };
}

namespace ImVid {
struct Assets::Data {
    std::map<Assets::Id, Assets::TexId> texIds;
};

Assets::Assets() : _data(new Data()) {
}

Assets::~Assets() {}

Assets::TexId Assets::getTexId(Id id) const {
    if (_data->texIds.find(id) == _data->texIds.end()) return 0;
    return _data->texIds.at(id);
}

bool Assets::load(const char * pathFolder) {
    // todo: refactor this in separate method
    // todo: load all remaining assets
    for (const auto & aid : kFilenameAsset) {
        int nx, ny, nz;
        const auto curAId = aid.first;

        std::string pathAsset(pathFolder);
        pathAsset += std::string("/") + ::kFilenameAsset.at(curAId);
        printf("Loading image '%s'\n", pathAsset.c_str());

        uint8_t * data = stbi_load(pathAsset.c_str(), &nx, &ny, &nz, STBI_rgb_alpha);

        if (data == nullptr) {
            fprintf(stderr, "Failed to load image\n");
            return false;
        }

        auto & curTexId = _data->texIds[curAId];

        GLint lastTexId;

        glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTexId);
        glGenTextures(1, &curTexId);
        glBindTexture(GL_TEXTURE_2D, curTexId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#ifdef GL_UNPACK_ROW_LENGTH // Not on WebGL/ES
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, nx, ny, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

        printf("Loaded successfully, texId = %d\n", curTexId);

        glBindTexture(GL_TEXTURE_2D, lastTexId);
    }

    return true;
}
}
