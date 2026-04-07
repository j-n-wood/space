#include "pages/base_page.h"
#include "assets/textures.h"
#include "assets/ui_elements.h"

extern "C" {    
    #include "raygui/raygui.h"
}

// button behaviour - standard buttons have target pages
const Page STANDARD_BUTTON_TARGET_PAGES[STANDARD_BUTTON_COUNT] = {
    PAGE_PRODUCTION,
    PAGE_ORBIT_STORES,
    PAGE_ORBIT_SHUTTLE_BAY,
    PAGE_ORBIT_SPACE_BAY,
    PAGE_SHUTTLE,
    PAGE_SELF_DESTRUCT, // could be a confirmation dialog or something
    PAGE_EARTH_TRAINING,
    PAGE_EARTH_RESEARCH,
    PAGE_SURFACE_SHUTTLE_BAY,
    PAGE_SURFACE_RESOURCES,
    PAGE_NONE,
    PAGE_SURFACE_STORES
};

// note - original button strip on left is 48 px wide from 320x256 screen size
// background images 272 x 168 - controls are 51 px wide so 3 px of background is unused?
// initial size set to 4x original

// original controls width = 51, height 120 of 256
// fit to buttom of page so 120 -> 480, top is 1024-480 = 544
Rectangle BasePage::sideBarDest = {0, 544, 51 * 4, 120 * 4}; // default value, will be set to actual screen size in main.cpp
Rectangle BasePage::mainScreenDest = {192, 0, 1280, 1024}; // default value, will be set to actual screen size in main.cpp

BasePage::BasePage() : backgroundTexture(nullptr), backgroundSource({0, 0, 0, 0}), standardButtons(ALL_STANDARD_BUTTONS) {
    // use textureManager to obtain default background texture
    backgroundTexture = TextureManager::getInstance().getTexture(TEXTURE_UI); // example, could set a default background here if desired
}

void BasePage::activate() {
    // default implementation does nothing, override in derived classes as needed
}

void BasePage::input() {
    // default input handler does nothing, override in derived classes as needed
}

void BasePage::render() {
    // default render handler draws the background if a texture is set
    if (backgroundTexture) {
        DrawTexturePro(*backgroundTexture, backgroundSource, mainScreenDest, (Vector2){0, 0}, 0.f, WHITE);
    }

    UITransparentButtonState transparentButtonState; // RAII helper to set transparent button styles for the duration of this render function

    renderStandardButtons(); // render the standard buttons based on the current bitfield
}

void BasePage::renderStandardButtons() {
    // example implementation to render a standard button strip on the left sidebar
    // assumes transparent render mode enabled
    
    // loop through standard buttons and render them if enabled in the bitfield
    // source rect => row = button index / 2

    // iterate flags in standardButtons bitfield
    for (int i = 0; i < STANDARD_BUTTON_COUNT; i++) {
        if (standardButtons & (1ULL << i)) {
            // button is enabled, render it
            GuiSetTooltip(standardButtonTooltips[i]); // set tooltip for this button
            if (GuiButton(standardButtonDestinations[i], "")) { // empty text since we're using a texture, could add text if desired
                // button was clicked, handle the event
                // look for standard target page for this button and switch to it if valid
                // TODO: ensure correct context (system, body, ship)
                Page targetPage = STANDARD_BUTTON_TARGET_PAGES[i];
                if (targetPage != PAGE_NONE) {
                    PageManager::getInstance().switchToPage(targetPage);   
                }
            }
        }
    }

}