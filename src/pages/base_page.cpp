#include "pages/base_page.h"
#include "assets/textures.h"
#include "assets/ui_elements.h"
#include "pages/overlay.h"
#include "state/game.h"

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
    PAGE_SURFACE_PRODUCTION,
    PAGE_SURFACE_STORES};

// note - original button strip on left is 48 px wide from 320x256 screen size
// background images 272 x 168 - controls are 51 px wide so 3 px of background is unused?
// initial size set to 4x original

// original controls width = 51, height 120 of 256
// fit to buttom of page so 120 -> 480, top is 1024-480 = 544
Rectangle BasePage::sideBarDest = {0, 544, 51 * 4, 120 * 4}; // default value, will be set to actual screen size in main.cpp
Rectangle BasePage::mainScreenDest = {192, 0, 1280, 1024};   // default value, will be set to actual screen size in main.cpp
Rectangle BasePage::timeDest = {100, 24, 1280 - 100, 10};

BasePage::BasePage() : backgroundTexture(nullptr), backgroundSource({0, 0, 0, 0}), standardButtons(ALL_STANDARD_BUTTONS)
{
    // use textureManager to obtain default background texture
    backgroundTexture = TextureManager::getInstance().getTexture(TEXTURE_UI); // example, could set a default background here if desired
}

void BasePage::activate(ViewState &viewState)
{
    // default implementation does nothing, override in derived classes as needed
}

void BasePage::input()
{
    // default input handler does nothing, override in derived classes as needed
}

void BasePage::render()
{
    // default render handler draws the background if a texture is set
    if (backgroundTexture)
    {
        DrawTexturePro(*backgroundTexture, backgroundSource, mainScreenDest, (Vector2){0, 0}, 0.f, WHITE);
    }

    // controls
    DrawTexturePro(*TextureManager::getInstance().getTexture(TEXTURE_UI_BUTTONS), uiElementSources[UI_CONTROLS], BasePage::sideBarDest, (Vector2){0, 0}, 0.f, WHITE);

    UITransparentButtonState transparentButtonState; // RAII helper to set transparent button styles for the duration of this render function

    renderStandardButtons(); // render the standard buttons based on the current bitfield
}

void BasePage::renderStandardButtons()
{
    // example implementation to render a standard button strip on the left sidebar
    // assumes transparent render mode enabled

    // loop through standard buttons and render them if enabled in the bitfield
    // source rect => row = button index / 2

    Overlay &overlay = Overlay::getInstance(); // get the overlay instance to set tooltips when hovering buttons

    // conditional enablement depends on focus location
    auto &vs{PageManager::getInstance().viewState};
    Location *location{vs.getCurrentLocation()};

    // can have orbital and/or surface facility
    Orbital *orbital{nullptr};
    ResourceFacility *surface{nullptr};
    if (location)
    {
        orbital = Game::getCurrent()->orbitalAt(location);
        surface = Game::getCurrent()->resourceFacilityAt(location);
    }

    // iterate flags in standardButtons bitfield
    for (int i = 0; i < STANDARD_BUTTON_COUNT; i++)
    {
        unsigned long long flag{1ULL << i};
        if (standardButtons & flag)
        {
            // conditional enablement
            bool enabled{false};

            switch (flag)
            {
            case BUTTON_PRODUCTION:
            case BUTTON_ORBIT_STORES:
            case BUTTON_ORBIT_SPACE_BAY:
            case BUTTON_ORBIT_SHUTTLE_BAY:
                enabled = (orbital != nullptr);
                break;
            case BUTTON_SURFACE_PRODUCTION:
                enabled = (surface != nullptr) && (surface->factory.get());
                break;
            case BUTTON_SHUTTLE:
                // must have one or other facility and a shuttle
                enabled = false; // (location != nullptr);
                break;
            case BUTTON_SURFACE_SHUTTLE_BAY:
            case BUTTON_SURFACE_RESOURCES:
            case BUTTON_SURFACE_STORES:
                enabled = (surface != nullptr);
                break;
            }

            if (!enabled)
            {
                continue;
            }

            // button is enabled, render it

            // render image

            // render button then tooltip
            int icon_index = standardButtonIcons[i];
            DrawTexturePro(*TextureManager::getInstance().getTexture(TEXTURE_UI_BUTTONS), facilityIconSources[icon_index], standardButtonDestinations[i], (Vector2){0, 0}, 0.f, WHITE);

            if (overlay.renderButton(standardButtonDestinations[i], "", standardButtonTooltips[i], WHITE))
            { // empty text since we're using a texture, could add text if desired
                // look for standard target page for this button and switch to it if valid
                // TODO: ensure correct context (system, body, ship)
                Page targetPage = STANDARD_BUTTON_TARGET_PAGES[i];
                if (targetPage != PAGE_NONE)
                {
                    PageManager::getInstance().switchToPage(targetPage);
                }
            }
        }
    }
}