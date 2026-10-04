#pragma once

#include "pages/modal.h"
#include "state/faction.h"

// view overlay for comms from another faction
class Craft;

class CommsView : public Modal
{
public:
    Craft *craft;
    Faction *faction;
    FactionInteraction state;

    CommsView(int l, int t);

    void initialise(Craft *c, Faction *f);
    void input();
    void render();
};