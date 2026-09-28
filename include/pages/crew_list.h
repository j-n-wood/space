#pragma once

#include "state/crew.h"
#include "state/facility.h"

#include "raygui/raygui.h"
#include "assets/ui_elements.h"

class CrewList
{
public:
    Facility *facility;
    Rectangle dest;
    bool visible;
    int scrollIndex;
    int itemActive;
    int itemFocused;

    Crew *activeCrews[MAX_BARRACKS_CREW];
    char nameBuffers[MAX_BARRACKS_CREW][64]; // buffers
    char *names[MAX_BARRACKS_CREW];          // pointers to buffers
    int currentCount;

    CrewList(const Rectangle &d) : facility{nullptr}, dest{d}, visible(false), scrollIndex{0}, itemActive{0}, itemFocused{0}, currentCount{0}
    {
        for (int i = 0; i < MAX_BARRACKS_CREW; i++)
        {
            names[i] = nameBuffers[i];
        }
    }

    void activate(Facility *f, Rectangle &target)
    {
        facility = f;
        dest = target;
    }

    int render()
    {
        // list items where each line is item.name, quantity in stores
        // do not list items with 0 quantity
        // for loading a pod, do not allow if item.pod_capacity is 0, as that indicates not loadable in pods
        // retain count of values
        currentCount = 0;
        for (int idx = 0; idx < MAX_BARRACKS_CREW; idx++)
        {
            Crew *crew = facility->barracks.crew[idx];
            if (crew)
            {
                activeCrews[currentCount] = crew;
                crew->description(names[currentCount], 64);
                currentCount++;
            }
        }

        GuiSetStyle(LISTVIEW, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
        GuiSetStyle(LISTVIEW, BASE_COLOR_NORMAL, ColorToInt(BLACK));
        GuiSetStyle(LISTVIEW, BASE_COLOR_FOCUSED, ColorToInt(DARKGRAY));
        GuiSetStyle(LISTVIEW, BASE_COLOR_PRESSED, ColorToInt(GRAY));

        GuiSetStyle(DEFAULT, BACKGROUND_COLOR, ColorToInt(BLACK));

        DefaultTextSizeState defaultTextSizeState(20);
        return GuiListViewEx(dest, names, currentCount, &scrollIndex, &itemActive, &itemFocused);
    }

    Crew *getFocusItem()
    {
        if (itemFocused >= 0 && itemFocused < currentCount)
        {
            return activeCrews[itemFocused];
        }
        return nullptr;
    }

    Crew *getSelectedItem()
    {
        if (itemActive >= 0 && itemActive < currentCount)
        {
            return activeCrews[itemActive];
        }
        return nullptr;
    }
};
