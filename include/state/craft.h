#pragma once

#include <cstdint>
#include "state/string_caps.h"

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

typedef enum
{
    AS_DISABLED, // not available
    AS_OFF,
    AS_ON,
    AS_COMPLETE, // finish cycle
    AS_COUNT
} AutopilotState;

class Autopilot
{
public:
    AutopilotState state;
    uint32_t fromSource; // bitfields of resource flags
    uint32_t fromDest;

    Autopilot() : state{AS_OFF}, fromSource{0}, fromDest{0} {};
};

class Pod
{
public:
};

class Location;

class Craft
{
public:
    char name[NAME_MAX_LEN];
    CraftState state;
    int state_timer;

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

    Craft(CraftState cs, uint8_t mp, Location *loc) : state{cs}, state_timer{0}, max_pods{mp}, drive{false}, location{loc} {};
};