#pragma once

#include "realtime_event.h"

class Location;

class CraftUnderAttackEvent : public CraftRealtimeEvent
{
    double time_until_damage;
    Location *initial_location;

public:
    CraftUnderAttackEvent(double duration, Craft *craft);

    bool cancelled();
    bool update(double delta);
    void onComplete();
};