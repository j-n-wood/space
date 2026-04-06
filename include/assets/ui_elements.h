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

class UITransparentButtonState {
    public:
    // helper class to set transparent button styles, can be used in any page render function before drawing buttons to make them transparent
    UITransparentButtonState() {
        // transparent buttons
        GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, 0x00000000);
        GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, 0x00000000); 
        GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, 0x00000000);

        // Hide Focused (Hover) State - Optional, if you want NO feedback
        GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED, 0x44776655);
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