#include "pages/stores_view.h"

#include "pages/resources.h"
#include "assets/ui_elements.h"
#include "state/game.h"
#include "state/stores.h"
#include "pages/base_page.h"

extern "C"
{
#include "raylib.h"
}

void StoresView::render()
{
    BasePage::render();

    // Nothing to list without stores. activate() leaves them null when this side of the
    // body has no facility.
    if (stores)
    {
        listResources();
        listItems();
    }
}

void StoresView::activate(ViewState &viewState)
{
    stores = nullptr;

    auto game{Game::getCurrent()};
    auto l{viewState.getCurrentBody()};

    Facility *f{nullptr};
    switch (side)
    {
    case LOCATION_TYPE_ORBIT:
        f = game->orbitalAt(l);
        break;
    case LOCATION_TYPE_SURFACE:
        f = game->resourceFacilityAt(l);
        break;
    default:
        break;
    }

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

void StoresView::listItems()
{
    if (!stores)
    {
        return;
    }
    Vector2 cursor{800, 64};

    // iterate available stores at facility
    char buf[256];
    for (auto &item : Game::getCurrent()->items)
    {
        if (item.id == 0)
        {
            // skip the 'empty' item // TODO special cases suck
            continue;
        }
        if (item.researched)
        {
            // emit the resource name
            DrawText(item.name, cursor.x, cursor.y, 20, WHITE);
            sprintf(buf, "%d", stores->items[item.id]);
            DrawText(buf, cursor.x + 120, cursor.y, 20, WHITE);
            cursor.y += 24;
        }
    }
}