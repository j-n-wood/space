#pragma once

#include "state/faction.h"

// view overlay for comms from another faction
class Craft;

class CommsView
{
public:
    Craft *craft;
    Faction *faction;
    bool visible;
    FactionInteraction state;
    int top;
    int left;

    CommsView(int l, int t);

    void activate(Craft *c, Faction *f);
    void deactivate();
    void input();
    void render();
};