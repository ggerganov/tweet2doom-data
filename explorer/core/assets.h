#pragma once

#include <cstdint>
#include <memory>

namespace ImVid {
class Assets {
public:
    Assets();
    ~Assets();

    enum Id {
        ICON_T2D_BIG,
        ICON_T2D_SMALL,
        ICON_T2D_SMALL_BLUR,
    };

    using TexId = uint32_t;

    TexId getTexId(Id id) const;

    bool load(const char * pathFolder);

private:
    struct Data;
    std::unique_ptr<Data> _data;
};
}
