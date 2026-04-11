#include "pages/stores_view.h"

#include "pages/resources.h"
#include "assets/ui_elements.h"
#include "state/game.h"
#include "state/stores.h"

extern "C"
{
#include "raylib.h"
}

void StoresView::render()
{
    BasePage::render();

    // controls
    DrawTexturePro(*TextureManager::getInstance().getTexture(TEXTURE_UI_BUTTONS), uiElementSources[UI_CONTROLS], BasePage::sideBarDest, (Vector2){0, 0}, 0.f, WHITE);

    // location resources
    if (auto location{Game::getInstance().getCurrentLocation()})
    {
        // what are the current stores?
        listResources();
    }
}

void StoresView::activate()
{
    stores = nullptr;
    // set current stores, if any
    auto f = Game::getInstance().getCurrentFacility();
    if (f)
    {
        stores = &f->stores;
    }
}

void StoresView::input()
{
    // no input handling for now, but could add some interactive elements here later
}

void StoresView::listResources()
{
    if (!stores)
    {
        return;
    }
    Vector2 cursor{400, 64};

    // iterate available stores at facility
    char buf[256];
    for (int idx = ResourceType::Iron; idx < ResourceType::Count; ++idx)
    {
        // emit the resource name
        DrawText(ResourceName[idx], cursor.x, cursor.y, 20, WHITE);
        sprintf(buf, "%d", stores->resources[idx]);
        DrawText(buf, cursor.x + 120, cursor.y, 20, WHITE);
        cursor.y += 24;
    }
}