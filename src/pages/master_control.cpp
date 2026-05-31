#include "pages/master_control.h"
#include "state/game.h"
#include "assets/ui_elements.h"
#include "pages/overlay.h"
#include "pages/pages.h"

#include <cstdio>

MasterControlView::MasterControlView()
{
    std::snprintf(title, sizeof title, "Master Control");
    backgroundTexture = nullptr; // no default background
}

void MasterControlView::activate(ViewState &viewState)
{
    currentSystem = viewState.getCurrentSystem();
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
        if (orbital->location->system == currentSystem)
        {
            // render it
            Rectangle target{x, y, 32 * 4, 16 * 4};
            DrawTexturePro(*texture_ui, uiElementSources[UI_ORBITAL_ICON_1], target, (Vector2){0, 0}, 0.f, WHITE);
            // add hover tooltip with location name
            if (overlay.clickedArea(target, orbital->location->name))
            {
                // switch to orbital page for this location
                PageManager &pm = PageManager::getInstance();
                pm.viewState.setCurrentLocation(orbital->location);
                pm.viewState.setCurrentFacility(orbital.get());
                pm.viewState.setCurrentCraft(nullptr);
                pm.switchToPage(PAGE_PRODUCTION); // orbital page has production info and access to factory, stores, etc.
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
        if (ios->location->system == currentSystem)
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
                pm.viewState.setCurrentLocation(ios->location);
                // see if there is a suitable facility at craft location
                if (Orbital *o = game->orbitalAt(ios->location))
                {
                    pm.viewState.setCurrentFacility(o);
                }
                else
                {
                    pm.viewState.setCurrentFacility(nullptr);
                }
                pm.viewState.setCurrentCraft(ios.get());
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