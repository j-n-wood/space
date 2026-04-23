#pragma once

#include "state/game.h"
#include "raygui/raygui.h"

class ItemList
{
public:
    Stores *stores;
    Game *game; // assumed that this will be reset by a call to activate() when data reloaded
    Rectangle dest;
    bool visible;
    int scrollIndex;
    int itemActive;
    int itemFocused;

    int activeItemsIDs[MAX_ITEM_TYPE];
    char nameBuffers[MAX_ITEM_TYPE][32]; // buffers
    char *names[MAX_ITEM_TYPE];          // pointers to buffers
    int currentCount;

    ItemList(const Rectangle &d) : stores{nullptr}, game{nullptr}, dest{d}, scrollIndex{0}, itemActive{0}, itemFocused{0}, currentCount{0}
    {
        for (int i = 0; i < MAX_ITEM_TYPE; i++)
        {
            names[i] = nameBuffers[i];
        }
    }

    void activate(Game *g, Stores *s)
    {
        game = g;
        stores = s;
    }

    int render()
    {
        // list items where each line is item.name, quantity in stores
        // do not list items with 0 quantity
        // for loading a pod, do not allow if item.pod_capacity is 0, as that indicates not loadable in pods
        // retain count of values
        currentCount = 0;
        for (auto &item : game->items)
        {
            if (stores->items[item.id] > 0 && item.pod_capacity > 0)
            {
                activeItemsIDs[currentCount] = item.id;
                std::snprintf(names[currentCount], 32, "%s: %d", item.name, stores->items[item.id]);
                currentCount++;
            }
        }

        GuiSetStyle(LISTVIEW, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
        GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
        GuiSetStyle(LISTVIEW, BASE_COLOR_NORMAL, ColorToInt(BLACK));
        GuiSetStyle(LISTVIEW, BASE_COLOR_FOCUSED, ColorToInt(DARKGRAY));
        GuiSetStyle(LISTVIEW, BASE_COLOR_PRESSED, ColorToInt(GRAY));

        GuiSetStyle(DEFAULT, BACKGROUND_COLOR, ColorToInt(BLACK));
        return GuiListViewEx(dest, names, currentCount, &scrollIndex, &itemActive, &itemFocused);
    }

    int getFocusItemID()
    {
        if (itemFocused >= 0 && itemFocused < currentCount)
        {
            return activeItemsIDs[itemFocused];
        }
        return -1;
    }

    int getSelectedItemID()
    {
        if (itemActive >= 0 && itemActive < currentCount)
        {
            return activeItemsIDs[itemActive];
        }
        return -1;
    }
};