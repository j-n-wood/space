#pragma once

#include <cstdint>
#include <memory>
#include "state/string_caps.h"
#include "state/waypoint.h"
#include "state/craft_type.h"

typedef enum
{
    CS_SURFACE, // surface no dock
    CS_SURFACE_DOCKED,
    CS_SURFACE_DOCK_WORK, // transient state while docked and working
    CS_SURFACE_WORK,
    CS_SURFACE_LAUNCH, // transient state leaving dock
    CS_ASCENDING,
    CS_ORBIT,
    CS_ORBIT_DOCKING, // transient state entering dock
    CS_ORBIT_DOCKED,
    CS_ORBIT_DOCK_WORK, // transient state while docked and working
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
    PT_WEAPON,
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

const int MAX_DESTINATIONS = 2;

class Autopilot;

class Craft
{
public:
    int id;
    int faction_id;

    char name[NAME_MAX_LEN];
    CraftType type;
    CraftState state;
    float state_timer;
    float total_state_timer; // full value of state timer, used to calculate progress for UI

    // crew

    // flight related
    int fuel;
    std::unique_ptr<Autopilot> autopilot;

    // parts and cargo
    uint8_t max_pods;
    Pod pods[6];
    bool drive; // fitted

    // current location if any
    Location *location;

    // destinations for transit
    Endpoint destinations[MAX_DESTINATIONS];

    // marker for next destination
    uint8_t destination_index;

    Craft(CraftState cs, uint8_t mp, Location *loc);
    virtual ~Craft();

    // The celestial body this craft is at. Today `location` is always a body so this
    // returns it unchanged; once locations become precise it walks up from a facility
    // or region. Out of line because location.h includes shuttle.h includes craft.h.
    Location *body() const;

    bool isPodEmpty(const int index);
    void setPodType(const int index, const PodType pt);
    virtual void update(float delta);

    const char *statusText(char *status, size_t len);

    inline void setTimedState(CraftState newState, float duration)
    {
        state = newState;
        state_timer = duration;
        total_state_timer = duration;
    }

    inline void setState(CraftState newState)
    {
        state = newState;
        state_timer = 0.0f;
        total_state_timer = 0.0f;
    }

    inline const Endpoint &currentDestination() const
    {
        return destinations[destination_index];
    }

    inline const Endpoint &priorDestination() const
    {
        return destinations[(destination_index + MAX_DESTINATIONS - 1) % MAX_DESTINATIONS];
    }

    // Am I where my current endpoint says? Out of line because it dereferences
    // Location, and location.h includes shuttle.h includes this header.
    bool atEndpoint() const;

    // runs when transit between locations is complete. Used to trigger game events.
    Craft &arriveAtLocation();

    inline Craft &nextEndpoint()
    {
        destination_index = (destination_index + 1) % MAX_DESTINATIONS;
        return *this;
    }

    Craft &engageDrive();

    Craft &disengageDrive();

    void setDestination(const uint8_t index, Location *loc);

    bool engageAutopilot();

    void disengageAutopilot();

    // Undock. Moves the craft out of the facility and back into the region containing
    // it -- a step up the hierarchy. Out of line because it dereferences Location.
    Craft &launch();

    // Move to this body's orbit or surface region: what ascending and descending
    // arrive at. A no-op if the body has no such region.
    void enterRegion(bool orbit);

    inline Craft &work(float duration)
    {
        if (state == CS_SURFACE_DOCKED)
        {
            state = CS_SURFACE_DOCK_WORK;
            total_state_timer = duration;
            state_timer = duration;
        }
        else if (state == CS_ORBIT_DOCKED)
        {
            state = CS_ORBIT_DOCK_WORK;
            total_state_timer = duration;
            state_timer = duration;
        }
        return *this;
    }

    // transition events
    virtual void onDocked();
    virtual void onDockWorkComplete();
};