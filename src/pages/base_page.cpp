#include "pages/base_page.h"
#include "wrappers/texture.h"
#include "assets/textures.h"

// note - original button strip on left is 48 px wide from 320x256 screen size

Rectangle BasePage::mainScreenDest = {0, 0, 1280, 1024}; // default value, will be set to actual screen size in main.cpp

BasePage::BasePage() : backgroundTexture(nullptr), backgroundSource({0, 0, 0, 0}) {
    // use textureManager to obtain default background texture
    backgroundTexture = TextureManager::getInstance().getTexture(TEXTURE_UI); // example, could set a default background here if desired
}

void BasePage::input() {
    // default input handler does nothing, override in derived classes as needed
}

void BasePage::render() {
    // default render handler draws the background if a texture is set
    if (backgroundTexture) {
        DrawTexturePro(*backgroundTexture, backgroundSource, mainScreenDest, (Vector2){0, 0}, 0.f, WHITE);
    }
}