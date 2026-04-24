#pragma once

#include "state/game.h"
#include "raygui/raygui.h"

class ResourceList
{
public:
    Game *game;
    Stores *stores;
    Rectangle dest;
    bool visible;
    int scrollIndex;
    int itemActive;
    int itemFocused;

    int activeResourceIDs[ResourceType::Count];
    char nameBuffers[ResourceType::Count][32]; // buffers
    char *names[ResourceType::Count];          // pointers to buffers

    ResourceList(Stores *s, const Rectangle &d) : stores{s}, dest{d}, scrollIndex{0}, itemActive{0}, itemFocused{0}
    {
        for (int i = 0; i < ResourceType::Count; i++)
        {
            names[i] = nameBuffers[i];
        }
    }

    void activate(Game *g, Stores *s, Rectangle &target)
    {
        game = g;
        stores = s;
        dest = target;
    }

    int render()
    {
        // list resources where each line is ResourceName
        // do not list resources with 0 quantity
        // retain count of values
        int count = 0;
        for (int i = 0; i < ResourceType::Count; i++)
        {
            if (stores->resources[i] > 0)
            {
                activeResourceIDs[count] = i;
                std::snprintf(names[count], 32, "%s: %d", ResourceName[i], stores->resources[i]);
                count++;
            }
        }

        GuiSetStyle(LISTVIEW, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
        GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
        GuiSetStyle(LISTVIEW, BASE_COLOR_NORMAL, ColorToInt(BLACK));
        GuiSetStyle(LISTVIEW, BASE_COLOR_FOCUSED, ColorToInt(DARKGRAY));
        GuiSetStyle(LISTVIEW, BASE_COLOR_PRESSED, ColorToInt(GRAY));

        GuiSetStyle(DEFAULT, BACKGROUND_COLOR, ColorToInt(BLACK));
        return GuiListViewEx(dest, names, count, &scrollIndex, &itemActive, &itemFocused);
    }

    int getFocusResourceID()
    {
        if (itemFocused >= 0 && itemFocused < ResourceType::Count)
        {
            return activeResourceIDs[itemFocused];
        }
        return -1;
    }

    int getSelectedResourceID()
    {
        if (itemActive >= 0 && itemActive < ResourceType::Count)
        {
            return activeResourceIDs[itemActive];
        }
        return -1;
    }
};