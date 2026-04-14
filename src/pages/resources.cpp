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

    // location resources
    listResources();
}

void Resources::activate(ViewState &viewState)
{
    auto game{Game::getCurrent()};
    location = viewState.getCurrentLocation();
    facility = game->resourceFacilityAt(location);
}

void Resources::input()
{
    // no input handling for now, but could add some interactive elements here later
}

void Resources::listResources()
{
    if (!location)
    {
        return;
    }
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