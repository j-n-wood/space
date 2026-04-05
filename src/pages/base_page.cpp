#include "pages/base_page.h"
#include "wrappers/texture.h"
#include "assets/textures.h"

// note - original button strip on left is 48 px wide from 320x256 screen size
// background images 272 x 168 - controls are 51 px wide so 3 px of background is unused?
// initial size set to 4x original

// original controls width = 51, height 120 of 256
// fit to buttom of page so 120 -> 480, top is 1024-480 = 544
Rectangle BasePage::sideBarDest = {0, 544, 51 * 4, 120 * 4}; // default value, will be set to actual screen size in main.cpp
Rectangle BasePage::mainScreenDest = {192, 0, 1280, 1024}; // default value, will be set to actual screen size in main.cpp

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