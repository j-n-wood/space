#pragma once

#include <cstdint>
#include "state/resources.h"

class Craft;

// autopilot is to manage resource transfers.
// On a suttle, it will be between surface and orbital facilities at the same location.
// On an IOS/SCG, it will be between an orbital at the origin and an orbital at the destination.

// Cursors exist to track what resources to transfer next. These are per facility in the route.
// If the route were to change (IOS changes destination), then the cursors should reset.

// autopilot with loop over endpoints on craft (currently 2).
// As autopilot is intended to move cargo, engaging autopilot will set endpoints to attempt docking.

typedef enum
{
    AS_DISABLED, // not available
    AS_OFF,
    AS_ON,
    AS_COMPLETE, // finish cycle
    AS_COUNT
} AutopilotState;

// Per-resource flow flags. Setting both flags ("balance") means the autopilot
// equalises the two stores rather than bulk-transferring in one direction.
enum ResourceFlow : uint8_t
{
    RF_NONE = 0,
    RF_LOAD_AT_SOURCE = 1 << 0, // load at CS_SURFACE_DOCKED
    RF_LOAD_AT_DEST = 1 << 1,   // load at CS_ORBIT_DOCKED
    RF_BALANCE = RF_LOAD_AT_SOURCE | RF_LOAD_AT_DEST,
};

class Autopilot
{
public:
    AutopilotState state;
    uint8_t flow[ResourceType::Count];
    uint8_t cursors[2]; // one cursor per endpoint, used for selecting next resource to transfer

    Autopilot();
    ~Autopilot();

    // Advances `cursor` and returns the next resource index whose flow & mask
    // is non-zero. Returns -1 if none are flagged. Wraps.
    int nextFlagged(uint8_t mask, uint8_t *cursor) const;

    // As above but also calls `accept(idx)` for each candidate and skips any
    // that return false. Lets the caller filter out resources with nothing
    // worth transferring so an otherwise-flagged pod does not end up empty.
    template <typename AcceptFn>
    int nextFlagged(uint8_t mask, uint8_t *cursor, AcceptFn accept) const
    {
        for (uint8_t step = 0; step < ResourceType::Count; ++step)
        {
            uint8_t idx = (*cursor + step) % ResourceType::Count;
            if ((flow[idx] & mask) && accept(idx))
            {
                *cursor = (idx + 1) % ResourceType::Count;
                return idx;
            }
        }
        return -1;
    }

    void update(Craft *craft, float delta);

    void onDocked(Craft *craft);
    void onDockWorkComplete(Craft *craft);
};
