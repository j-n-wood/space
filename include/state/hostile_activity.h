#pragma once

#include <cstdint>

class Craft;
class Facility;

// list of ongoing hostile actions, initially by faction 1 only

// approach to an orbital
// boarding action at an orbital
// craft under realtime attack - not listed as scanning events is faster

enum class HostileActivityType : uint8_t
{
    HAT_APPROACH_ORBITAL,
    HAT_BOARDING_ORBITAL,
    HAT_REALTIME_ATTACK
};

class HostileActivity
{
public:
    HostileActivityType type;
    Craft *origin;
    Facility *target;

    HostileActivity(HostileActivityType t, Craft *o, Facility *f)
        : type(t), origin(o), target(f) {}
};