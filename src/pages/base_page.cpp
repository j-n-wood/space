#include "pages/base_page.h"
#include "wrappers/texture.h"

Rectangle BasePage::mainScreenDest = {0, 0, 1280, 800}; // default value, will be set to actual screen size in main.cpp

void BasePage::input() {
    // default input handler does nothing, override in derived classes as needed
}

void BasePage::render() {
    // default render handler draws the background if a texture is set
    if (backgroundTexture) {
        DrawTexturePro(*backgroundTexture, backgroundSource, mainScreenDest, (Vector2){0, 0}, 0.f, WHITE);
    }
}