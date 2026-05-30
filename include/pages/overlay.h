#pragma once

extern "C"
{
#include "raylib.h"
}

using onHover = void (*)(void *);

class Overlay
{
    const char *currentToolTip = nullptr;

public:
    Overlay();

    void start();
    void render();
    int renderButton(const Rectangle &buttonRect, const char *buttonText, const char *toolTip, const Color &color);
    int renderButtonHover(const Rectangle &buttonRect, const char *buttonText, const Color &color, onHover hover, void *state);
    bool clickedArea(const Rectangle &area, const char *toolTip); // basically transparent button with hovertext, no outline
    void setDefaultStyle();

    inline void setCurrentToolTip(const char *toolTip)
    {
        if (toolTip != currentToolTip)
        {
            currentToolTip = toolTip;
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