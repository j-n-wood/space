#pragma once
#include <cstdint>
#include <string>

#include "assets/textures.h" // easy access to predefined texture assets
#include "pages/pages.h"     // for page background sources
#include "pages/view_state.h"

extern "C"
{
#include "raylib.h"
}

class Location;

// bitfield for standard button enablement and visibility
// EC has training and research where others have orbit marker

enum StandardButton
{
    BUTTON_NONE = 0,
    BUTTON_PRODUCTION = 1 << 0,
    BUTTON_ORBIT_STORES = 1 << 1,
    BUTTON_ORBIT_SHUTTLE_BAY = 1 << 2,
    BUTTON_ORBIT_SPACE_BAY = 1 << 3,
    BUTTON_SHUTTLE = 1 << 4,
    BUTTON_SELF_DESTRUCT = 1 << 5,
    BUTTON_TRAINING = 1 << 6,
    BUTTON_RESEARCH = 1 << 7,
    BUTTON_SURFACE_SHUTTLE_BAY = 1 << 8,
    BUTTON_SURFACE_RESOURCES = 1 << 9,
    BUTTON_SURFACE_PRODUCTION = 1 << 10,
    BUTTON_SURFACE_STORES = 1 << 11,
    // add more buttons as needed
};

const int STANDARD_BUTTON_COUNT = 12; // update this if you add more buttons

// default page has all bar training and research
const uint64_t ALL_STANDARD_BUTTONS = BUTTON_PRODUCTION | BUTTON_ORBIT_STORES | BUTTON_ORBIT_SHUTTLE_BAY | BUTTON_ORBIT_SPACE_BAY |
                                      BUTTON_SHUTTLE | BUTTON_SELF_DESTRUCT |
                                      BUTTON_SURFACE_SHUTTLE_BAY | BUTTON_SURFACE_RESOURCES | BUTTON_SURFACE_PRODUCTION | BUTTON_SURFACE_STORES;

class BasePage
{
public:
    const TextureAsset *backgroundTexture;
    Rectangle backgroundSource;
    uint64_t standardButtons; // bitfield to track which standard buttons are active
    std::string title;        // page title

    BasePage();
    virtual ~BasePage() {}
    virtual void activate(ViewState &viewState); // on change to this page, set any required state
    virtual void render();
    virtual void input();

    void renderStandardButtons();

    static Rectangle mainScreenDest;
    static Rectangle sideBarDest; // space for control bar
    static Rectangle timeDest;    // time display, drawn by overlay

    static void setWindowSize(int width, int height)
    {
        // update the static rectangles based on the new window size
        sideBarDest = {0, (float)(height - 120 * 4), 51 * 4, 120 * 4};  // keep sidebar width fixed at 192
        mainScreenDest = {192, 0, (float)(width - 192), (float)height}; // main screen takes the rest of the width
        timeDest = {(float)(width - 100), 10, 100, 24};                 // top right
    }
};