#pragma once

#include <cstdint>
#include "state/string_caps.h"
#include "state/autopilot.h"

typedef enum
{
    CS_SURFACE, // surface no dock
    CS_SURFACE_DOCKED,
    CS_SURFACE_WORK,
    CS_SURFACE_LAUNCH, // transient state leaving dock
    CS_ASCENDING,
    CS_ORBIT,
    CS_ORBIT_DOCKING, // transient state entering dock
    CS_ORBIT_DOCKED,
    CS_ORBIT_WORK,
    CS_ORBIT_LAUNCH, // transient state leaving dock
    CS_DESCENDING,
    CS_TRANSIT, // IP or IS transit - refine with type and speed
    CS_COUNT
} CraftState;

// default state times
const float CSTD_ASCENT = 4.0f;
const float CSTD_DESCENT = 2.0f;
const float CSTD_LAUNCH = 0.3f;
const float CSTD_DOCK = 0.3f;

typedef enum
{
    PT_EMPTY,
    PT_TOOL,
    PT_SUPPLY,
    PT_CRYO,
    PT_COUNT
} PodType;

class Pod
{
public:
    PodType type;
    int contentType; // item index or resource type
    int amount;      // amount or count

    Pod() : type{PT_EMPTY}, contentType{0}, amount{0} {};

    const char *description(char *dest, size_t len);
};

class Location;

class Craft
{
public:
    char name[NAME_MAX_LEN];
    CraftState state;
    float state_timer;

    // crew

    // flight related
    int fuel;
    Autopilot autopilot;

    // parts and cargo
    uint8_t max_pods;
    Pod pods[6];
    bool drive; // fitted

    // current location if any
    Location *location;

    Craft(CraftState cs, uint8_t mp, Location *loc) : state{cs}, state_timer{0.0f}, max_pods{mp}, drive{false}, location{loc}
    {
        name[0] = '\0';
    };
    virtual ~Craft() = default;

    bool isPodEmpty(const int index);
    void setPodType(const int index, const PodType pt);
    virtual void update(float delta);
};