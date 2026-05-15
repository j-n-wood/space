#include "pages/autopilot_view.h"
#include "state/craft.h"
#include "pages/overlay.h"

extern "C"
{
#include "raylib.h"
#include "raygui/raygui.h"
}

void AutopilotView::input()
{
    if (IsKeyPressed(KEY_X))
    {
        // enable/disable autopilot
    }
}

void AutopilotView::render()
{
    char status[128];

    static Color autopilotColors[AS_COUNT] = {
        [AS_DISABLED] = GRAY,
        [AS_OFF] = RED,
        [AS_ON] = GREEN,
        [AS_COMPLETE] = YELLOW,
    };

    static Color flowColors[4] = {
        [0] = GRAY,
        [1] = GREEN,
        [2] = RED,
        [3] = YELLOW,
    };

    GuiPanel((Rectangle){320, 25, 600, 600}, "Autopilot");
    DrawText(autopilotStateNames[autopilot->state], 360, 80, 20, autopilotColors[autopilot->state]);

    // buttons to change state, draw via overlay to assign tooltip
    auto &overlay{Overlay::getInstance()};
    // toggle button text depending on state
    const char *button_text = (autopilot->state == AS_OFF) ? "Enable Autopilot" : "Disable Autopilot";
    if (overlay.renderButton((Rectangle){640, 80, 150, 30}, button_text, "Enable or disable the autopilot system", autopilotColors[autopilot->state]))
    {
        if (autopilot->state == AS_OFF)
        {
            craft->engageAutopilot();
        }
        else if (autopilot->state == AS_ON)
        {
            craft->disengageAutopilot();
        }
    }

    // show destinations
    if (!craft)
    {
        return;
    }
    char dest_desc[128];
    DrawText(craft->currentDestination().description(dest_desc, sizeof dest_desc), 320, 50, 20, GREEN);
    DrawText(craft->priorDestination().description(dest_desc, sizeof dest_desc), 640, 50, 20, RED);

    // list resources and show flow state. List in reverse order s.t. drop down is not hidden by later drawing.

    int y_start = 100;
    int item_height = 30;

    for (int i = ResourceType::Iron; i < ResourceType::Count; i++)
    {
        float y_pos = (float)(y_start + i * item_height);
        DrawText(ResourceName[i], 340, (float)(y_pos + 5), 20, flowColors[autopilot->flow[i]]);
    }

    // store active value so that altering it does not immediately change state
    int current_active = toggled_resource;
    // 1. Draw all standard (closed) dropdowns first
    // if we have an open dropdown, disable all the others
    if (toggled_resource != -1)
    {
        GuiDisable();
    }

    for (int i = ResourceType::Iron; i < ResourceType::Count; i++)
    {
        // Skip the one that is currently toggled/active
        if (current_active == i)
            continue;

        float y_pos = (float)(y_start + i * item_height);
        Rectangle rect = {440, y_pos, 125, (float)item_height};

        // Force editMode to false so they act like normal buttons/labels
        if (GuiDropdownBox(rect, "NONE;LOAD AT SOURCE;LOAD AT DESTINATION;BALANCE", &autopilot->flow[i], false))
        {
            toggled_resource = i; // Open this one
        }
    }

    // enable GUI again for the active dropdown
    if (current_active != -1)
    {
        GuiEnable();
    }

    // Draw the ACTIVE dropdown last so it appears on top and gets priority
    if (current_active != -1)
    {
        float y_pos = (float)(y_start + current_active * item_height);
        Rectangle rect = {440, y_pos, 125, (float)item_height};

        if (GuiDropdownBox(rect, "NONE;LOAD AT SOURCE;LOAD AT DESTINATION;BALANCE", &autopilot->flow[current_active], true))
        {
            toggled_resource = -1; // Close it when item selected or clicked away
        }
    }
}