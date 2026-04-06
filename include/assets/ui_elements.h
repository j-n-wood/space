#pragma once

#include "wrappers/texture.h"

extern "C" { 
    #include "raygui/raygui.h"
}

typedef enum {
    UI_EARTH_CITY_CONTROLS,
    UI_CONTROLS,
    UI_ELEMENT_COUNT
} UIElementID;

const Rectangle uiElementSources[UI_ELEMENT_COUNT] = {
    {174, 23, 51, 120},
    {230, 23, 51, 120},
};

// standard buttons are from 175, 24 (TL) width 24, height 17, 2 columns, 6 rows
enum UIStandardButton {    
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
    UI_BUTTON_EMPTY_10,
    UI_BUTTON_SURFACE_STORES,
    UI_BUTTON_COUNT
};

// source coordinates
const Rectangle standardButtonRects[UI_BUTTON_COUNT] = {
    {175, 24, 24, 17},
    {199, 24, 24, 17},
    {175, 41, 24, 17},
    {199, 41, 24, 17},
    {175, 58, 24, 17},
    {199, 58, 24, 17},
    {175, 75, 24, 17},
    {199, 75, 24, 17},
    {175, 92, 24, 17},
    {199, 92, 24, 17},
    {175, 109, 24, 17},
    {199, 109, 24, 17}
};

// display coordinates - based on top = 1024 - 120*4, width is 24 *4, height is 17*4, 4 pix boundary on left, center and right, and between rows
const Rectangle standardButtonDestinations[UI_BUTTON_COUNT] = {
    {0, 1024 - 120*4, 24*4, 17*4},
    {24*4 + 4, 1024 - 120*4, 24*4, 17*4},
    {0, 1024 - (120-17)*4, 24*4, 17*4},
    {24*4 + 4, 1024 - (120-17)*4, 24*4, 17*4},
    {0, 1024 - (120-34)*4, 24*4, 17*4},
    {24*4 + 4, 1024 - (120-34)*4, 24*4, 17*4},
    {0, 1024 - (120-51)*4, 24*4, 17*4},
    {24*4 + 4, 1024 - (120-51)*4, 24*4, 17*4},
    {0, 1024 - (120-68)*4, 24*4, 17*4},
    {24*4 + 4, 1024 - (120-68)*4, 24*4, 17*4},
    {0, 1024 - (120-85)*4, 24*4, 17*4},
    {24*4 + 4, 1024 - (120-85)*4, 24*4, 17*4}
};

class UITransparentButtonState {
    public:
    // helper class to set transparent button styles, can be used in any page render function before drawing buttons to make them transparent
    UITransparentButtonState() {
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

    ~UITransparentButtonState() {
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