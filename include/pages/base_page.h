#pragma once

extern "C" {
    #include "raylib.h"
}

class TextureAsset;

class BasePage
{
public:
    TextureAsset* backgroundTexture;
    Rectangle     backgroundSource;

    virtual ~BasePage() {}
    virtual void render();
    virtual void input();

    BasePage& setBackground(TextureAsset* texture, Rectangle source) {
        this->backgroundTexture = texture;
        this->backgroundSource = source;
        return *this;
    }

    static Rectangle mainScreenDest;
};