#pragma once

#include "assets/textures.h" // easy access to predefined texture assets

extern "C" {
    #include "raylib.h"
}

class BasePage
{
public:
    const TextureAsset* backgroundTexture;
    Rectangle     backgroundSource;

    BasePage();
    virtual ~BasePage() {}
    virtual void render();
    virtual void input();

    static Rectangle mainScreenDest;
    static Rectangle sideBarDest;   // space for control bar

    static void setWindowSize(int width, int height) {
        // update the static rectangles based on the new window size
        sideBarDest = {0, (float)(height - 120 * 4), 51*4, 120 * 4}; // keep sidebar width fixed at 192
        mainScreenDest = {192, 0, (float)(width - 192), (float)height}; // main screen takes the rest of the width
    }
};