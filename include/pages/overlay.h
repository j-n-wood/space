#pragma once

#include <cstring>

extern "C"
{
#include "raylib.h"
}

using onHover = void (*)(void *);

class Overlay
{
    bool toolTipSet;
    char currentToolTip[256] = {0}; // copy of input in case it is a shared buffer and changes

    char consoleInput[256] = {0};

public:
    bool console;

    Overlay();

    void start();
    void render();
    void input();

    int renderButton(const Rectangle &buttonRect, const char *buttonText, const char *toolTip, const Color &color);
    int renderButtonHover(const Rectangle &buttonRect, const char *buttonText, const Color &color, onHover hover, void *state);
    bool clickedArea(const Rectangle &area, const char *toolTip); // basically transparent button with hovertext, no outline
    void setDefaultStyle();

    inline void setCurrentToolTip(const char *toolTip)
    {
        if (toolTip != currentToolTip)
        {
            std::strncpy(currentToolTip, toolTip, sizeof(currentToolTip) - 1);
            currentToolTip[sizeof(currentToolTip) - 1] = '\0'; // ensure null termination
            toolTipSet = true;
        }
    }

    bool addToolTip(const char *toolTip, const Rectangle &hoverArea)
    {
        if (toolTip != nullptr && CheckCollisionPointRec(GetMousePosition(), hoverArea))
        {
            setCurrentToolTip(toolTip);
            return true;
        }
        return false; // not shown
    }

    // singleton pattern
    static Overlay &getInstance()
    {
        static Overlay instance; // Guaranteed to be destroyed, instantiated on first use.
        return instance;
    }
};