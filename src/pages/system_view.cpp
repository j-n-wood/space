#include <cstdlib>

#include "pages/pages.h"
#include "pages/system_view.h"
#include "assets/ui_elements.h"
#include "pages/pages.h"
#include "state/game.h"

extern "C"
{
#include "raylib.h"
#include "raygui/raygui.h"
}

void bodySelected(void *state, Location *loc)
{
    // callback from orrery when a location is selected - bring up resource info
    auto *svState = static_cast<SystemView *>(state);
    svState->setSelectedLocation(loc);
}

void SystemView::activate(ViewState &viewState)
{
    // attach to current system
    if (System *currentSystem = PageManager::getInstance().viewState.getCurrentSystem())
    {
        setSystem(currentSystem);
        // set callback for body selection in orrery
        orrery->caller = this;
        orrery->onDestinationSelectedCallback = bodySelected;
    }
}

void SystemView::render()
{
    BasePage::render();

    orrery->render();

    renderLocationInfo();
}

void SystemView::renderLocationInfo()
{
    if (selectedLocation)
    {
        // render info about the selected location, e.g. resources, facilities, etc.

        // draw a 50% black rectangle as background for text
        DrawRectangle(980, 30, 250, 400, (Color){0, 0, 0, 128});

        DrawText(selectedLocation->name, 1000, 50, 20, WHITE);

        char buf[64];
        int idx = 0;
        for (auto i = ResourceType::Iron; i < ResourceType::MehFuel; ++i)
        {
            if (selectedLocation->resources.availability[i] == 0)
            {
                continue; // skip resources that are not available at this location
            }
            std::snprintf(buf, sizeof buf, "%s: %d", ResourceName[i], selectedLocation->resources.availability[i]);
            DrawText(buf, 1000, 70 + idx++ * 20, 20, WHITE);
        }
    }
}

void SystemView::input()
{
    orrery->input();

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
    {
        selectedLocation = nullptr; // clear selection on right click in orrery
    }
}
