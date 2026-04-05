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
};