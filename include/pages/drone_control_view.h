#pragma once

class Craft;

enum DroneControlState
{
    DCS_MANAGE,
    DCS_BATTLE,
    DCS_MAX
};

class DroneControlView
{
    void render_manage();
    void render_battle();

public:
    Craft *craft;
    bool visible;
    DroneControlState state;

    DroneControlView(Craft *c) : craft(c), visible(false), state(DCS_MANAGE) {}

    void activate(Craft *c);
    void deactivate();

    void input();
    void render();
};