#pragma once

#include "state/autopilot.h"

class AutopilotView
{
public:
    Autopilot *autopilot;
    Craft *craft;
    bool visible;
    int toggled_resource;

    AutopilotView(Autopilot *ap, Craft *c) : autopilot(ap), craft(c), visible(false), toggled_resource(-1) {}

    void input();
    void render();
};