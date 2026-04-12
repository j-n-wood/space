#include "pages/pages.h"
#include "pages/resources.h"
#include "assets/ui_elements.h"
#include "state/game.h"

extern "C"
{
#include "raylib.h"
}

void Resources::render()
{
    BasePage::render();

    // controls
    DrawTexturePro(*TextureManager::getInstance().getTexture(TEXTURE_UI_BUTTONS), uiElementSources[UI_CONTROLS], BasePage::sideBarDest, (Vector2){0, 0}, 0.f, WHITE);

    // location resources
    if (auto location{Game::getCurrent()->getCurrentLocation()})
    {
        listResources(location);
    }
}

void Resources::activate()
{
    auto game{Game::getCurrent()};
    facility = game->resourceFacilityAt(game->getCurrentLocation());
}

void Resources::input()
{
    // no input handling for now, but could add some interactive elements here later
}

void Resources::listResources(const Location *location)
{
    Vector2 cursor{400, 64};

    // iterate available resources at location
    for (int idx = 0; idx < ResourceType::Count; ++idx)
    {
        if (location->resources.availability[idx] > 0)
        {
            // emit the resource name
            DrawText(ResourceName[idx], cursor.x, cursor.y, 20, WHITE);
            cursor.y += 24;
        }
    }

    // derricks
    char buf[256];
    sprintf(buf, "Derricks: %d", facility->num_derricks);
    DrawText(buf, 400, cursor.y, 20, WHITE);
}