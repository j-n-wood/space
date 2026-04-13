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

    inline void setCurrentToolTip(const char *toolTip)
    {
        if (toolTip != currentToolTip)
        {
            currentToolTip = toolTip;
        }
    }

    // singleton pattern
    static Overlay &getInstance()
    {
        static Overlay instance; // Guaranteed to be destroyed, instantiated on first use.
        return instance;
    }
};