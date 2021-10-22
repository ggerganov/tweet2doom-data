#pragma once

namespace ImVid {

struct Canvas {
    float posX;
    float posY;
    float sizeX;
    float sizeY;

    float viewCenterX = 0.5f;
    float viewCenterY = 0.5f;
    float viewSizeX = 1.0f;
    float viewSizeY = 1.0f;

    // helper methods

    template <class T> T getPos() const { return { posX, posY }; }
    template <class T> T getSize() const { return { sizeX, sizeY }; }

    template <class T> void setPos(const T & pos) { posX = pos.x; posY = pos.y; }
    template <class T> void setSize(const T & size) { sizeX = size.x; sizeY = size.y; }

    template <class T> T getPosInCanvas(float x, float y) {
        T res(x, y);
        res.x -= posX;
        res.y -= posY;
        res.x /= sizeX;
        res.y /= sizeY;

        return res;
    }

    template <class T> T getPosInCanvas(T xy) {
        return getPosInCanvas<T>(xy.x, xy.y);
    }

    template <class T> T getPosInImage(float x, float y) {
        T res = getPosInCanvas<T>(x, y);

        res.x = res.x*viewSizeX + viewCenterX - 0.5f*viewSizeX;
        res.y = res.y*viewSizeY + viewCenterY - 0.5f*viewSizeY;

        return res;
    }

    template <class T> T getPosInImage(T xy) {
        return getPosInImage<T>(xy.x, xy.y);
    }
};

}
