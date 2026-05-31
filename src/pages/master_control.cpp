#include "pages/master_control.h"
#include "state/game.h"
#include "assets/ui_elements.h"
#include "pages/overlay.h"
#include "pages/pages.h"

#include <cstdio>
#include <cmath>

MasterControlView::MasterControlView() : faction_id(0), currentSystem(nullptr)
{
    std::snprintf(title, sizeof title, "Master Control");
    backgroundTexture = nullptr; // no default background
}

void MasterControlView::activate(ViewState &viewState)
{
    currentSystem = viewState.getCurrentSystem();
    faction_id = viewState.getFactionId();

    // clear animation state
    for (int i = 0; i < 32; ++i)
    {
        orbitalAnimTime[i] = 0.0f;
    }
}

void MasterControlView::update(const float delta)
{
    // update animation state
    for (int i = 0; i < 32; ++i)
    {
        orbitalAnimTime[i] += delta;
        if (orbitalAnimTime[i] > 1.0f)
        {
            orbitalAnimTime[i] -= 1.0f;
        }
    }
}

void MasterControlView::render()
{
    // render background and standard buttons as normal
    BasePage::render();

    renderOrbitals();
    renderIOS();
}

void MasterControlView::renderOrbitals()
{
    static auto texture_ui = TextureManager::getInstance().getTexture(TEXTURE_UI);

    Game *game = Game::getCurrent();
    Overlay &overlay = Overlay::getInstance();

    // render orbitals in this system
    // filtering all is a bit inefficient - but picks up changes automatically

    int count = 0;
    float x = 240.0;
    float y = 130;
    for (auto &orbital : game->allOrbitals())
    {
        // is it in the current system?
        if ((orbital->location->system == currentSystem) && (orbital->faction_id == faction_id))
        {
            // render it
            Rectangle target{x, y, 32 * 4, 16 * 4};
            auto frame = static_cast<int>(UI_ORBITAL_ICON_1);
            if (orbital->factory && orbital->factory->isActive())
            {
                float progress = 3.0f * orbitalAnimTime[count];    // 3 frames
                frame = UI_ORBITAL_ICON_1 + (int)floorf(progress); // active icon
            }
            DrawTexturePro(*texture_ui, uiElementSources[frame], target, (Vector2){0, 0}, 0.f, WHITE);
            // add hover tooltip with location name
            if (overlay.clickedArea(target, orbital->location->name))
            {
                // switch to orbital page for this location
                PageManager &pm = PageManager::getInstance();
                pm.viewState.setFacilityFocus(orbital.get());
                pm.switchToPage(PAGE_ORBITAL); // orbital page has production info and access to factory, stores, etc.
            }
            y += 80.0; // Move to the next position
            ++count;
            if (count > 10)
            {
                // next column
                count = 0;
                y = 130.0;
                x += 160.0;
            }
        }
    }
}

void MasterControlView::renderIOS()
{
    static auto texture_ui_buttons = TextureManager::getInstance().getTexture(TEXTURE_UI_BUTTONS);

    // similar to renderOrbitals but for IOS, and show different info on hover
    Game *game = Game::getCurrent();
    Overlay &overlay = Overlay::getInstance();

    // render orbitals in this system
    // filtering all is a bit inefficient - but picks up changes automatically

    int count = 0;
    float x = 640.0;
    float y = 130;
    for (auto &ios : game->allIOS())
    {
        // is it in the current system?
        if ((ios->location->system == currentSystem) && (ios->faction_id == faction_id))
        {
            // render it
            Rectangle target{x, y, 32 * 4, 16 * 4};

            // map craft state to icon
            const Rectangle *source = nullptr;
            switch (ios->state)
            {
            case CS_ORBIT:
                source = &uiElementSources[UI_BUTTON_CRAFT_ORBIT];
                break;
            case CS_ORBIT_DOCKED:
                source = &uiElementSources[UI_BUTTON_CRAFT_DOCKED];
                break;
            case CS_ORBIT_LAUNCH:
                source = &uiElementSources[UI_BUTTON_CRAFT_LAUNCHING];
                break;
            case CS_ORBIT_WORK:
                source = &uiElementSources[UI_BUTTON_CRAFT_MINING];
                break;
            case CS_ORBIT_DOCKING:
                source = &uiElementSources[UI_BUTTON_CRAFT_DOCKING];
                break;
            case CS_TRANSIT:
                source = &uiElementSources[UI_BUTTON_CRAFT_TRANSIT];
                break;
            default:
                source = &uiElementSources[UI_BUTTON_CRAFT_TRANSIT];
                break;
            }

            DrawTexturePro(*texture_ui_buttons, *source, target, (Vector2){0, 0}, 0.f, WHITE);
            // add hover tooltip with craft name, state
            char buf[192];
            std::snprintf(buf, sizeof buf, "%s - %s", ios->name, ios->statusText(buf + 64, sizeof buf - 64));
            if (overlay.clickedArea(target, buf))
            {
                // switch to orbital page for this location
                PageManager &pm = PageManager::getInstance();
                pm.viewState.setCraftFocus(ios.get());
                pm.switchToPage(PAGE_SHUTTLE);
            }
            y += 80.0; // Move to the next position
            ++count;
            if (count > 10)
            {
                // next column
                count = 0;
                y = 130.0;
                x += 160.0;
            }
        }
    }
}