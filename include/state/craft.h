#pragma once

#include <cstdint>
#include <memory>
#include "state/string_caps.h"
#include "state/waypoint.h"
#include "state/craft_type.h"
#include "state/craft_action.h"
#include "state/object.h"

typedef enum
{
    CS_IDLE,       // at rest wherever location says: region or facility, orbit or surface
    CS_WORKING,    // pods tick; docked = at a station, undocked = building one
    CS_LAUNCHING,  // leaving a facility
    CS_ASCENDING,  // surface region -> orbit region
    CS_DESCENDING, // orbit region -> surface region
    CS_DOCKING,    // approaching a facility
    CS_TRANSIT,    // between bodies; location is the system's `space`
    CS_SCANNING,   // scanning a location for objects
    CS_COUNT
} CraftState;

// default state times
const float CSTD_ASCENT = 4.0f;
const float CSTD_DESCENT = 2.0f;
const float CSTD_LAUNCH = 0.3f;
const float CSTD_DOCK = 0.3f;
const float CSTD_SCANNING = 3.0f;

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
    Object *object;  // optional: id of held object for grapple

    Pod() : type{PT_EMPTY}, contentType{0}, amount{0}, object{nullptr} {};

    const char *description(char *dest, size_t len);
};

const int MAX_DESTINATIONS = 2;

class Autopilot;

class Crew;

class CurrentState
{
public:
    CraftState state;
    float state_timer;
    float total_state_timer;
};

class Craft
{
protected:
    CraftState state;
    float state_timer;
    float total_state_timer; // full value of state timer, used to calculate progress for UI
public:
    int id;
    int faction_id;

    char name[NAME_MAX_LEN];
    CraftType type;

    // crew

    // flight related
    int fuel;
    std::unique_ptr<Autopilot> autopilot;

    // parts and cargo
    uint8_t max_pods;
    Pod pods[6];
    int8_t active_pod_index; // a pod that is working, -1 -> none
    bool drive;              // fitted

    // current location if any
    Location *location;

    // destinations for transit
    Endpoint destinations[MAX_DESTINATIONS];

    // marker for next destination
    uint8_t destination_index;

    // scan target
    Object *scan_object; // optional: object being scanned

    // crew
    Crew *crew; // optional: crew assigned to this craft

    Craft(CraftState cs, uint8_t mp, Location *loc);
    virtual ~Craft();

    // The celestial body this craft is at. Today `location` is always a body so this
    // returns it unchanged; once locations become precise it walks up from a facility
    // or region. Out of line because location.h includes shuttle.h includes craft.h.
    Location *body() const;

    bool isPodEmpty(const int index);
    void setPodType(const int index, const PodType pt);
    void update(float delta);

    const char *statusText(char *status, size_t len);
    const char *scanTargetText(char *status, size_t len);

    inline void setTimedState(CraftState newState, float duration)
    {
        if (state == CS_SCANNING && newState != CS_SCANNING)
        {
            // stop scanning
            stopScanning();
        }
        state = newState;
        state_timer = duration;
        total_state_timer = duration;
    }

    inline void setState(CraftState newState)
    {
        if (newState == state)
        {
            return;
        }
        if (state == CS_SCANNING)
        {
            // stop scanning
            stopScanning();
        }
        state = newState;
        state_timer = 0.0f;
        total_state_timer = 0.0f;
    }

    // set state from persistence/tests
    inline Craft &assignState(CraftState newState, float current, float duration)
    {
        state = newState;
        state_timer = current;
        total_state_timer = duration;
        return *this;
    }

    // copy of state for persistence
    inline CurrentState currentState() const
    {
        return {state, state_timer, total_state_timer};
    }

    inline float stateProgress() const
    {
        if (total_state_timer > 0.0f)
        {
            return 1.0f - (state_timer / total_state_timer);
        }
        return 1.0f;
    }

    inline bool hasCapability(CraftCapability cap) const
    {
        return craftHasCapability(type, cap);
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

    CraftActionResult canDock() const;
    CraftActionResult canLaunch() const;
    CraftActionResult canAscend() const;
    CraftActionResult canDescend() const;
    CraftActionResult canEngageDrive() const;
    CraftActionResult canWork() const;
    CraftActionResult canEngageAutopilot() const;

    Craft &engageDrive();

    Craft &disengageDrive();

    void setDestination(const uint8_t index, Location *loc);

    CraftActionResult engageAutopilot();
    CraftActionResult disengageAutopilot();

    // Undock. Moves the craft out of the facility and back into the region containing
    // it -- a step up the hierarchy. Out of line because it dereferences Location.
    Craft &launch();

    Craft &dock(); // move from orbit to docked at a facility

    Craft &ascend();  // move from surface to orbit
    Craft &descend(); // move from orbit to surface

    // Move to this body's orbit or surface region: what ascending and descending
    // arrive at. A no-op if the body has no such region.
    void enterRegion(bool orbit);

    // action related

    // Where, straight from the hierarchy -- they ARE the craft's position, so they
    // cannot contradict it. Out of line because they dereference Location, as launch()
    // is for the same reason.
    bool docked() const;
    bool inOrbit() const;
    inline bool inTransit() const { return state == CS_TRANSIT; }
    inline bool isLaunching() const { return state == CS_LAUNCHING; }
    inline bool isDocking() const { return state == CS_DOCKING; }
    inline bool working() const { return state == CS_WORKING; }
    inline bool scanning() const { return state == CS_SCANNING; }
    inline bool idle() const { return state == CS_IDLE; }

    bool moving() const
    {
        switch (state)
        {
        case CS_LAUNCHING:
        case CS_ASCENDING:
        case CS_DESCENDING:
        case CS_DOCKING:
        case CS_TRANSIT:
            return true;
        case CS_IDLE:
        case CS_WORKING:
        case CS_SCANNING:
        case CS_COUNT:
            return false;
        }
        return false;
    }

    inline Craft &work(float duration)
    {
        if (canWork())
        {
            setTimedState(CS_WORKING, duration);
        }
        return *this;
    }

    Craft &startScanning();
    Craft &stopScanning();

    inline Crew *assignCrew(Crew *c)
    {
        auto prior = crew;
        crew = c;
        return prior;
    }

    // transition events
    virtual void onDocked();
};