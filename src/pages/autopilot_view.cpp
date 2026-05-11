#include "pages/autopilot_view.h"
#include "state/craft.h"

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

    /*
        BORDER_COLOR_NORMAL = 0,    // Control border color in STATE_NORMAL
    BASE_COLOR_NORMAL,          // Control base color in STATE_NORMAL
    TEXT_COLOR_NORMAL,          // Control text color in STATE_NORMAL
    BORDER_COLOR_FOCUSED,       // Control border color in STATE_FOCUSED
    BASE_COLOR_FOCUSED,         // Control base color in STATE_FOCUSED
    TEXT_COLOR_FOCUSED,         // Control text color in STATE_FOCUSED
    BORDER_COLOR_PRESSED,       // Control border color in STATE_PRESSED
    BASE_COLOR_PRESSED,         // Control base color in STATE_PRESSED
    TEXT_COLOR_PRESSED,         // Control text color in STATE_PRESSED
    BORDER_COLOR_DISABLED,      // Control border color in STATE_DISABLED
    BASE_COLOR_DISABLED,        // Control base color in STATE_DISABLED
    */

    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, 0x663333ff);
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, 0x442222ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0xCCCCCCff);

    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, 0x885555ff);
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, 0x664444ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, 0xCCCCCCff);

    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, 0xAA8888ff);
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, 0x885555ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, 0xCCCCCCff);

    GuiSetStyle(DEFAULT, BORDER_COLOR_DISABLED, 0x442222ff);
    GuiSetStyle(DEFAULT, BASE_COLOR_DISABLED, 0x442222ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED, 0x888888ff);

    GuiSetStyle(COMBOBOX, BORDER_COLOR_NORMAL, 0x663333ff);
    GuiSetStyle(COMBOBOX, BASE_COLOR_NORMAL, 0x442222ff);
    GuiSetStyle(COMBOBOX, TEXT_COLOR_NORMAL, 0xCCCCCCff);

    GuiSetStyle(COMBOBOX, BORDER_COLOR_FOCUSED, 0x885555ff);
    GuiSetStyle(COMBOBOX, BASE_COLOR_FOCUSED, 0x664444ff);
    GuiSetStyle(COMBOBOX, TEXT_COLOR_FOCUSED, 0xCCCCCCff);

    GuiSetStyle(COMBOBOX, BORDER_COLOR_PRESSED, 0xAA8888ff);
    GuiSetStyle(COMBOBOX, BASE_COLOR_PRESSED, 0x885555ff);
    GuiSetStyle(COMBOBOX, TEXT_COLOR_PRESSED, 0xCCCCCCff);

    GuiSetStyle(COMBOBOX, BORDER_COLOR_DISABLED, 0x442222ff);
    GuiSetStyle(COMBOBOX, BASE_COLOR_DISABLED, 0x442222ff);
    GuiSetStyle(COMBOBOX, TEXT_COLOR_DISABLED, 0x888888ff);

    GuiSetStyle(DROPDOWNBOX, BORDER_COLOR_NORMAL, 0x663333ff);
    GuiSetStyle(DROPDOWNBOX, BASE_COLOR_NORMAL, 0x442222ff);
    GuiSetStyle(DROPDOWNBOX, TEXT_COLOR_NORMAL, 0xCCCCCCff);

    GuiSetStyle(DROPDOWNBOX, BORDER_COLOR_FOCUSED, 0x885555ff);
    GuiSetStyle(DROPDOWNBOX, BASE_COLOR_FOCUSED, 0x664444ff);
    GuiSetStyle(DROPDOWNBOX, TEXT_COLOR_FOCUSED, 0xCCCCCCff);

    GuiSetStyle(DROPDOWNBOX, BORDER_COLOR_PRESSED, 0xAA8888ff);
    GuiSetStyle(DROPDOWNBOX, BASE_COLOR_PRESSED, 0x885555ff);
    GuiSetStyle(DROPDOWNBOX, TEXT_COLOR_PRESSED, 0xCCCCCCff);

    GuiSetStyle(DROPDOWNBOX, BORDER_COLOR_DISABLED, 0x442222ff);
    GuiSetStyle(DROPDOWNBOX, BASE_COLOR_DISABLED, 0x442222ff);
    GuiSetStyle(DROPDOWNBOX, TEXT_COLOR_DISABLED, 0x888888ff);

    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, 0x000000ff);
    GuiSetStyle(COMBOBOX, BACKGROUND_COLOR, 0x000000ff);
    GuiSetStyle(DROPDOWNBOX, BACKGROUND_COLOR, 0x000000ff);
    GuiSetStyle(LISTVIEW, BACKGROUND_COLOR, 0x000000ff);

    GuiSetStyle(DEFAULT, LINE_COLOR, 0x663333ff);
    GuiSetStyle(COMBOBOX, LINE_COLOR, 0x663333ff);
    GuiSetStyle(DROPDOWNBOX, LINE_COLOR, 0x663333ff);
    GuiSetStyle(LISTVIEW, LINE_COLOR, 0x663333ff);

    GuiPanel((Rectangle){320, 25, 600, 600}, "Autopilot");
    DrawText(autopilotStateNames[autopilot->state], 360, 70, 20, autopilotColors[autopilot->state]);

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

    // 2. Draw the ACTIVE dropdown last so it appears on top and gets priority
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