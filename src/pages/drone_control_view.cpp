#include "pages/drone_control_view.h"

extern "C"
{
#include "raylib.h"
}

void DroneControlView::activate(Craft *c)
{
    craft = c;
    visible = true;
    state = DCS_MANAGE;
}

void DroneControlView::deactivate()
{
    craft = nullptr;
    visible = false;
}

void DroneControlView::input()
{
    if (!visible)
    {
        return;
    }

    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
    {
        // toggle between manage and battle mode on right click
        deactivate();
    }
}

void DroneControlView::render()
{
    if (!visible)
    {
        return;
    }

    switch (state)
    {
    case DCS_MANAGE:
        render_manage();
        break;
    case DCS_BATTLE:
        render_battle();
        break;
    default:
        TraceLog(LOG_ERROR, "Invalid drone control state");
        break;
    }
}

void DroneControlView::render_manage()
{
    DrawText("Drone Control - Manage", 10, 50, 20, WHITE);
}

void DroneControlView::render_battle()
{
    DrawText("Drone Control - Battle", 10, 50, 20, WHITE);
}