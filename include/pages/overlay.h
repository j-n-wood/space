#pragma once

#include <cstring>

extern "C"
{
#include "raylib.h"
}

using onHover = void (*)(void *);

class RepeatButtonState
{
public:
    bool held{false};
    float add_rate{1.0f};
    float dead_time{0.0f};
    float acceleration{0.3f}; // rate of increase in add_rate per second
    const Rectangle *last_button{nullptr};

    RepeatButtonState(const float accel = 0.3f) : acceleration{accel} {}

    void reset()
    {
        held = false;
        add_rate = 1.0f;
        dead_time = 0.0f;
        last_button = nullptr;
    }
    int rate() const
    {
        return static_cast<int>(add_rate); // clamp to int
    }
    void update(const float delta);
};

class Overlay
{
    bool toolTipSet;
    char currentToolTip[256] = {0}; // copy of input in case it is a shared buffer and changes

    char consoleInput[256] = {0};

public:
    bool console = false;
    bool debug = false; // debug-tools flag; gates BasePage::renderDebug (toggled by F10)
    bool help = false;  // help text flag; gates BasePage::showHelp (toggled by H)

    Overlay();

    void start();
    void render();
    void input();

    int renderButton(const Rectangle &buttonRect, const char *buttonText, const char *toolTip, const Color &color);
    int renderButtonHover(const Rectangle &buttonRect, const char *buttonText, const Color &color, onHover hover, void *state);
    bool clickedArea(const Rectangle &area, const char *toolTip);                             // basically transparent button with hovertext, no outline
    bool mouseDownArea(const Rectangle &area, const char *toolTip, RepeatButtonState *state); // basically transparent button with hovertext, no outline, but returns true if mouse is down
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