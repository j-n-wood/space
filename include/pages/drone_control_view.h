#pragma once

#include "state/item.h"

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
    ItemType droneType;
    bool visible;
    DroneControlState state;
    int top;
    int left;

    DroneControlView(int l, int t) : craft(nullptr), droneType(ItemType::MAX_ITEM_TYPE), visible(false), state(DCS_MANAGE), top(t), left(l) {}

    void activate(Craft *c);
    void deactivate();

    void input();
    void render();
};