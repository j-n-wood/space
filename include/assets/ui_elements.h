#pragma once

#include "wrappers/texture.h"

extern "C"
{
#include "raygui/raygui.h"
}

typedef enum
{
    UI_CONTROLS, // empty control frame
    UI_ELEMENT_COUNT
} UIElementID;

const Rectangle uiElementSources[UI_ELEMENT_COUNT] = {
    {174, 23, 51, 120},
};

// standard buttons are from 175, 24 (TL) width 24, height 17, 2 columns, 6 rows
enum UIStandardButton
{
    UI_BUTTON_PRODUCTION,
    UI_BUTTON_ORBIT_STORES,
    UI_BUTTON_ORBIT_SHUTTLE_BAY,
    UI_BUTTON_ORBIT_SPACE_BAY,
    UI_BUTTON_SHUTTLE,
    UI_BUTTON_SELF_DESTRUCT,
    UI_BUTTON_TRAINING,
    UI_BUTTON_RESEARCH,
    UI_BUTTON_SURFACE_SHUTTLE_BAY,
    UI_BUTTON_SURFACE_RESOURCES,
    UI_BUTTON_SURFACE_PRODUCTION,
    UI_BUTTON_SURFACE_STORES,
    UI_BUTTON_COUNT
};

// display coordinates - based on top = 1024 - 120*4, width is 24 *4, height is 17*4, 4 pix boundary on left, center and right, and between rows
// inner button image is 20 x 12, then 1px black, 1px green border then 1px grey frame
// therefore 3x3 border

const int border = 3;
const int top = 256 - 120;
const int col2 = 25;
const int button_width = 20;
const int button_height = 12;
const int row_height = 17;

const Rectangle standardButtonDestinations[UI_BUTTON_COUNT] = {
    {4 * border, 4 * (top + border), button_width * 4, button_height * 4},
    {4 * (border + col2), 4 * (top + border), button_width * 4, button_height * 4},

    {4 * border, 4 * (top + border + row_height), button_width * 4, button_height * 4},
    {4 * (border + col2), 4 * (top + border + row_height), button_width * 4, button_height * 4},

    {4 * border, 4 * (top + border + 2 * row_height), button_width * 4, button_height * 4},
    {4 * (border + col2), 4 * (top + border + 2 * row_height), button_width * 4, button_height * 4},

    {4 * border, 4 * (top + border + 3 * row_height), button_width * 4, button_height * 4},
    {4 * (border + col2), 4 * (top + border + 3 * row_height), button_width * 4, button_height * 4},

    {4 * border, 4 * (top + border + 4 * row_height), button_width * 4, button_height * 4},
    {4 * (border + col2), 4 * (top + border + 4 * row_height), button_width * 4, button_height * 4},

    {4 * border, 4 * (top + border + 5 * row_height), button_width * 4, button_height * 4},
    {4 * (border + col2), 4 * (top + border + 5 * row_height), button_width * 4, button_height * 4},
};

// facility icons
typedef enum
{
    FI_PRODUCTION,
    FI_STORES,
    FI_SHUTTLE_BAY,
    FI_SPACE_BAY,
    FI_SHUTTLE,
    FI_SELF_DESTRUCT,
    FI_TRAINING,
    FI_RESEARCH,
    FI_RESOURCES,
    FI_ORBITAL,
    FI_SURFACE,
    FI_COUNT
} FacilityIcon;

// source coordinates
const Rectangle facilityIconSources[FI_COUNT] = {
    {233, 26, 20, 12},
    {258, 26, 20, 12},
    {233, 43, 20, 12},
    {258, 43, 20, 12},
    {233, 60, 20, 12},
    {258, 60, 20, 12},
    {232, 9, 20, 12},
    {258, 9, 20, 12},
    {258, 94, 20, 12},
    {231, 75, 48, 16},
    {231, 126, 48, 16},
};

const int standardButtonIcons[UI_BUTTON_COUNT] = {
    0, 1, 2, 3, 4, 5, 6, 7, 2, 8, 0, 1};

extern const char *standardButtonTooltips[UI_BUTTON_COUNT];

class UITransparentButtonState
{
public:
    // helper class to set transparent button styles, can be used in any page render function before drawing buttons to make them transparent
    UITransparentButtonState()
    {
        // transparent buttons
        GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, 0x00000000);
        GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, 0x00000000);
        GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, 0x00000000);

        // Hide Focused (Hover) State - Optional, if you want NO feedback
        GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED, 0x77998877);
        GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, 0x00000000);
        GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED, 0x00000000);

        // Hide Pressed State
        GuiSetStyle(BUTTON, BORDER_COLOR_PRESSED, 0x00000055);
        GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, 0x00000055);
        GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED, 0x00000000);
    }

    ~UITransparentButtonState()
    {
        // reset to default styles if needed, or just rely on the fact that styles are global and will be overridden by other pages as needed
        GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, 0x447766FF);
        GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, 0x559977FF);
        GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, 0xFFFFFFFF);

        // Focused (Hover) State - Optional, if you want NO feedback
        GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED, 0x447766FF);
        GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, 0x77BB99FF);
        GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED, 0xFFFFFFFF);

        // Pressed State
        GuiSetStyle(BUTTON, BORDER_COLOR_PRESSED, 0x225544FF);
        GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, 0x337755FF);
        GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED, 0xFFFFFFFF);
    }
};