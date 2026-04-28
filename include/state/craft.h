#pragma once

#include <cstdint>
#include <memory>
#include "state/string_caps.h"
#include "state/waypoint.h"

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
    CT_SHUTTLE,
    CT_IOS,
    CT_SCG,
    CT_COUNT
} CraftType;

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

const int MAX_DESTINATIONS = 2;

class Autopilot;

class Craft
{
public:
    char name[NAME_MAX_LEN];
    CraftType type;
    CraftState state;
    float state_timer;

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

    bool isPodEmpty(const int index);
    void setPodType(const int index, const PodType pt);
    virtual void update(float delta);

    const char *statusText(char *status, size_t len);

    inline const Endpoint &currentDestination() const
    {
        return destinations[destination_index];
    }

    // test if at endpoint
    inline bool atEndpoint() const
    {
        const auto &current_dest{destinations[destination_index]};
        if (current_dest.location != location)
        {
            return false;
        }
        if (current_dest.sublocation == SLOC_ORBIT)
        {
            if (current_dest.docked)
            {
                return state == CS_ORBIT_DOCKED;
            }
            return state == CS_ORBIT;
        }

        return state == CS_SURFACE_DOCKED || state == CS_SURFACE;
    }

    // runs when transit between locations is complete. Used to trigger game events.
    inline Craft &arriveAtLocation()
    {
        auto &current_dest{destinations[destination_index]};
        location = current_dest.location;
        if (atEndpoint())
        {
            nextEndpoint();
        }
        return *this;
    }

    inline Craft &nextEndpoint()
    {
        destination_index = (destination_index + 1) % MAX_DESTINATIONS;
        return *this;
    }

    inline Craft &engageDrive()
    {
        if (destinations[destination_index].location)
        {
            state = CS_TRANSIT;
            state_timer = 10.0f; // TODO transit time could be based on distance and drive type
        }
        return *this;
    }

    void setDestination(const uint8_t index, Location *loc);

    bool engageAutopilot();

    void disengageAutopilot();

    inline Craft &launch()
    {
        if (state == CS_SURFACE_DOCKED)
        {
            // last_at_source = true;
            state = CS_SURFACE_LAUNCH;
            state_timer = CSTD_LAUNCH;
        }
        else if (state == CS_ORBIT_DOCKED)
        {
            // last_at_source = false;
            state = CS_ORBIT_LAUNCH;
            state_timer = CSTD_LAUNCH;
        }
        return *this;
    }

    inline Craft &work(float duration)
    {
        if (state == CS_SURFACE_DOCKED)
        {
            state = CS_SURFACE_DOCK_WORK;
            state_timer = duration;
        }
        else if (state == CS_ORBIT_DOCKED)
        {
            state = CS_ORBIT_DOCK_WORK;
            state_timer = duration;
        }
        return *this;
    }

    // transition events
    virtual void onDocked();
    virtual void onDockWorkComplete();
};