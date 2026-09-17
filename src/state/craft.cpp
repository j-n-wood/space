#include <cstdio>
#include <cmath>
#include "state/craft.h"
#include "state/game.h"
#include "state/resources.h"
#include "state/autopilot.h"
#include "state/craft_action.h"

const char *PodTypeName[PT_COUNT] = {
    "EMPTY",
    "TOOL",
    "SUPPLY",
    "CRYO",
    "WEAPON"};

const char *Pod::description(char *dest, size_t len)
{
    switch (type)
    {
    case PT_TOOL:
        if (amount)
        {
            if (amount > 1)
            {
                std::snprintf(dest, len, "%s (%d)", Game::getCurrent()->items[contentType].name, amount);
            }
            else
            {
                std::snprintf(dest, len, "%s", Game::getCurrent()->items[contentType].name);
            }
        }
        else
        {
            std::snprintf(dest, len, "Tool Pod");
        }
        break;
    case PT_SUPPLY:
        if (amount)
        {
            if (amount > 1)
            {
                std::snprintf(dest, len, "%s (%d)", ResourceName[contentType], amount);
            }
            else
            {
                std::snprintf(dest, len, "%s", ResourceName[contentType]);
            }
        }
        else
        {
            std::snprintf(dest, len, "Supply Pod");
        }
        break;
    case PT_CRYO:
        // TODO
        break;
    case PT_WEAPON:
        std::snprintf(dest, len, "%s", Game::getCurrent()->items[contentType].name);
        break;
    default:
        std::snprintf(dest, len, "EMPTY");
        break;
    }
    return dest;
}

Craft::Craft(CraftState cs, uint8_t mp, Location *loc) : id{0}, faction_id{0}, state{cs}, state_timer{0.0f}, max_pods{mp}, drive{false}, location{loc}, destination_index{0}, autopilot{std::make_unique<Autopilot>()}
{
    name[0] = '\0';
};

Craft::~Craft()
{
}

Location *Craft::body() const
{
    return location ? location->body() : nullptr;
}

bool Craft::docked() const
{
    return location && location->isFacility();
}

bool Craft::inOrbit() const
{
    return location && location->inOrbit();
}

CraftActionResult Craft::canAscend() const
{
    if (!hasCapability(CC_ATMOSPHERIC))
    {
        return CAC_NOT_CAPABLE;
    } // tier 1
    if (!drive)
    {
        return CAC_NO_DRIVE;
    } // tier 2
    if (moving() || working())
    {
        return CAC_BUSY;
    } // tier 3
    if (!location->isOnSurface())
    {
        return CAC_WRONG_STATE;
    }
    return CAC_OK;
}

CraftActionResult Craft::canDescend() const
{
    if (!hasCapability(CC_ATMOSPHERIC))
    {
        return CAC_NOT_CAPABLE;
    } // tier 1
    if (!drive)
    {
        return CAC_NO_DRIVE;
    } // tier 2
    if (moving() || working())
    {
        return CAC_BUSY;
    } // tier 3
    if (location->type != LOCATION_TYPE_ORBIT)
    {
        return CAC_WRONG_STATE;
    }
    return CAC_OK;
}

CraftActionResult Craft::canEngageDrive() const
{
    if (!hasCapability(CC_INTERPLANETARY))
    {
        return CAC_NOT_CAPABLE;
    } // tier 1
    if (!drive)
    {
        return CAC_NO_DRIVE;
    } // tier 2
    if (moving() || working())
    {
        return CAC_BUSY;
    } // tier 3
    if (docked())
    {
        return CAC_WRONG_STATE; // still made fast to a station -- launch first
    }
    if (!currentDestination().location)
    {
        return CAC_NO_DESTINATION; // engageDrive would silently do nothing
    }
    return CAC_OK;
}

CraftActionResult Craft::canWork() const
{
    if (moving() || working())
    {
        return CAC_BUSY;
    }
    return CAC_OK;
}

CraftActionResult Craft::canDock() const
{
    if (moving() || working())
    {
        return CAC_BUSY;
    }

    bool can_dock = false;
    if (location->type != LOCATION_TYPE_ORBIT)
    {
        return CAC_NO_ORBITAL;
    }
    auto game = Game::getCurrent();

    Orbital *o = game->orbitalAt(location);

    if (!o)
    {
        return CAC_NO_ORBITAL;
    }

    if (!o->operational)
    {
        return CAC_ORBITAL_INCOMPLETE;
    }

    // TODO - if dock is occupied

    if (o->faction_id != faction_id)
    {
        if (game->factions[o->faction_id].hostile)
        {
            int drone_count = o->stores.items[ItemType::Star_Drone] + o->stores.items[ItemType::Ios_Drone];
            if (drone_count > 0)
            {
                return CAC_DEFENDED;
            }
        }
    }
    return CAC_OK;
}

CraftActionResult Craft::canLaunch() const
{
    if (!drive)
    {
        return CAC_NO_DRIVE;
    }

    if (moving() || working())
    {
        return CAC_BUSY;
    }

    if (!location->isFacility())
    {
        return CAC_WRONG_STATE;
    }

    return CAC_OK;
}

Craft &Craft::launch()
{
    if (!canLaunch())
    {
        return *this;
    }

    setTimedState(CS_LAUNCHING, CSTD_LAUNCH);

    // Leaving a facility puts us back in the region it sits in.
    // assume construction is correct
    location = location->primary;

    return *this;
}

Craft &Craft::dock()
{
    // Only start the approach; onDocked() steps into the facility when the timer runs
    // out. Moving now would make docked() true for the whole manoeuvre, so the craft
    // would report itself arrived before it was.
    if (canDock())
    {
        setTimedState(CS_DOCKING, CSTD_DOCK);
    }
    return *this;
}

Craft &Craft::ascend() // move from surface to orbit, can initiate from any surface type
{
    if (canAscend())
    {
        // Starting from a station is an instant launch into the climb: step out into
        // the surface region first, or the craft would read as docked for the whole
        // ascent -- inside a building and airborne at once.
        if (location->isFacility())
        {
            location = location->primary;
        }
        setTimedState(CS_ASCENDING, CSTD_ASCENT);
    }
    return *this;
}

Craft &Craft::descend() // move from orbit to surface, can only initiate from orbit
{
    if (canDescend())
    {
        setTimedState(CS_DESCENDING, CSTD_DESCENT);
    }
    return *this;
}

void Craft::enterRegion(bool orbit)
{
    Location *b = body();
    if (!b)
    {
        return;
    }
    Location *region = orbit ? b->orbit() : b->surface();
    if (region)
    {
        location = region;
    }
}

bool Craft::atEndpoint() const
{
    const Endpoint &dest = destinations[destination_index];
    if (!dest.location || !location)
    {
        return false;
    }

    // Both sides name a precise location, so orbit, surface and docked are all implied
    // by which one it is.
    if (dest.location == location)
    {
        return true;
    }

    // One tolerance: sent to a region, and ended up docked at a station inside it.
    // Descending auto-docks when a station is there, so this counts as arrival rather
    // than leaving the autopilot circling.
    return location->isFacility() && location->primary == dest.location;
}

bool Craft::isPodEmpty(const int index)
{
    if (index >= max_pods)
    {
        return true;
    }
    if (pods[index].type == PT_EMPTY)
    {
        return true;
    }
    return pods[index].amount == 0;
}

void Craft::setPodType(const int index, const PodType pt)
{
    if (index < max_pods)
    {
        pods[index].type = pt;
    }
}

void Craft::update(float delta)
{
    // state transitions

    // timed states
    if (state_timer > 0.0f)
    {
        state_timer -= delta;
        if (state_timer <= 0)
        {
            state_timer = 0.0f;
            const CraftState expiring = state;
            state = CS_IDLE; // most states come to rest; the arms below adjust position
            switch (expiring)
            {
            case CS_WORKING:
                if (docked())
                {
                    onDockWorkComplete();
                }
                // onDocked(); // TODO - immediately dock if you completed a faciltity?
                break;
            case CS_LAUNCHING:
                // Leaving the ground is only the first phase of a climb -- a shuttle
                // cannot sit in the surface region under power -- so launching there
                // continues into ascent. Launching from an orbital is already done:
                // it comes to rest in the orbit region. The 14-state model got this
                // from CS_SURFACE_LAUNCH being a different value to CS_ORBIT_LAUNCH;
                // with one CS_LAUNCHING the place has to say which it was.
                if (!inOrbit())
                {
                    setTimedState(CS_ASCENDING, CSTD_ASCENT);
                }
                break;
            case CS_ASCENDING:
                enterRegion(true); // reached orbit
                break;
            case CS_DESCENDING:
                // Reached the ground either way; onDocked steps into the station if
                // there is one to dock at.
                enterRegion(false);
                if (Game::getCurrent()->resourceFacilityAt(location))
                {
                    onDocked();
                }
                break;
            case CS_DOCKING:
                onDocked();
                if (hasCapability(CC_INTERPLANETARY)) // not triggered for shuttles ATM
                {
                    Game::getCurrent()->onSpacecraftDocked(this);
                }
                break;
            case CS_TRANSIT:
                arriveAtLocation();
                Game::getCurrent()->onSpacecraftArrival(this);
                break;
            default:
                break;
            }
        }
    } // timed state

    // update autopilot if fitted
    if (drive)
    {
        // update autopilot logic here
        autopilot->update(this, delta);
    }

    // working states
    if (state == CS_WORKING)
    {
        Game *game = Game::getCurrent();
        for (int pod_idx = 0; pod_idx < max_pods; ++pod_idx)
        {
            if (!isPodEmpty(pod_idx))
            {
                game->updateActivePod(this, pods[pod_idx], delta);
            }
        }
    }
}

Craft &Craft::arriveAtLocation()
{
    auto &current_dest{destinations[destination_index]};

    // Transit ends in orbit, so arrive at the destination body's orbit region rather
    // than at the body itself. Docking, if the endpoint wants it, happens next.
    Location *target = current_dest.location;
    if (target)
    {
        Location *b = target->body();
        location = (b && b->orbit()) ? b->orbit() : target;
    }

    if (atEndpoint())
    {
        TraceLog(LOG_INFO, "Arrived at destination: %s", location ? location->name : "Space");
        nextEndpoint();
    }
    return *this;
}

void Craft::onDocked()
{
    // Docking moves the craft INTO the facility: it is a child of the region we were
    // in, so this is a step down the hierarchy rather than a lookup.
    Location *b = body();
    if (b)
    {
        Game *game = Game::getCurrent();
        Facility *f = inOrbit() ? static_cast<Facility *>(game->orbitalAt(b))
                                : static_cast<Facility *>(game->resourceFacilityAt(b));
        if (f)
        {
            location = f;
        }
    }

    if (atEndpoint())
    {
        autopilot->onDocked(this); // called before advancing endpoint, current dest = where we are now
        nextEndpoint();
    }
}

void Craft::onDockWorkComplete()
{
    // pass onto autopilot to update its state if working, e.g. to advance supply flow
    if (autopilot->state >= AS_ON)
    {
        autopilot->onDockWorkComplete(this);
    }
}

const char *Craft::statusText(char *status, size_t len)
{
    // A craft is always somewhere; "nowhere in particular" is a system's space location.
    // The name carries the noun -- "Earth Orbit", "Earth Surface", "Earth Orbital" --
    // so these strings supply only the verb. Adding one back doubles it.
    const char *location_name = location ? location->name : "Space";

    switch (state)
    {
    // You are *at* a station, *in* an orbit and *on* a surface. The preposition is the
    // last thing the collapsed enum stopped carrying, so it comes from the place.
    case CS_IDLE:
        if (docked())
        {
            std::snprintf(status, len, "Docked at %s", location_name);
        }
        else
        {
            std::snprintf(status, len, inOrbit() ? "In %s" : "On %s", location_name);
        }
        break;
    case CS_DOCKING:
        std::snprintf(status, len, "Docking at %s", location_name);
        break;
    case CS_WORKING:
        if (docked())
        {
            std::snprintf(status, len, "Working at %s", location_name);
        }
        else
        {
            std::snprintf(status, len, inOrbit() ? "Working in %s" : "Working on %s", location_name);
        }
        break;
    case CS_LAUNCHING:
        std::snprintf(status, len, "Launching from %s", location_name);
        break;
    case CS_ASCENDING:
        std::snprintf(status, len, "Ascending from %s", location_name);
        break;
    case CS_DESCENDING:
        std::snprintf(status, len, "Descending from %s", location_name);
        break;
    case CS_TRANSIT:
    {
        auto &destination{destinations[destination_index]};
        if (destination.location)
        {
            std::snprintf(status, len, "In transit to %s", destination.location->name);
        }
        else
        {
            std::snprintf(status, len, "In transit");
        }
    }
    break;
    default:
        break;
    }

    return status;
}

Craft &Craft::engageDrive()
{
    auto canEngage = canEngageDrive();
    if (!canEngage)
    {
        TraceLog(LOG_WARNING, "Cannot engage drive on %s : %s", name, canEngage.text());
        return *this;
    }
    if (destinations[destination_index].location)
    {
        auto source = location;
        auto destination = destinations[destination_index].location;

        // make up a transit time based on location distance
        Game *game = Game::getCurrent();

        float transit_time = game->transitTimeCalculator->calculateTransitTime(source, destination);

        TraceLog(LOG_INFO, "Engaging drive from %s to %s, transit time %.1f seconds", source ? source->name : "Space", destination->name, transit_time);

        setTimedState(CS_TRANSIT, transit_time);
        location = location->system->space; // space location for system
    }
    return *this;
}

Craft &Craft::disengageDrive()
{
    if (state == CS_TRANSIT)
    {
        TraceLog(LOG_INFO, "Disengaging drive");
        // TODO
    }
    return *this;
}

void Craft::setDestination(const uint8_t index, Location *loc)
{
    if (index >= MAX_DESTINATIONS)
    {
        return;
    }
    // The picker still offers bodies, so resolve to an exact place: the orbital if
    // there is one, otherwise the body's orbit region. Naming the station IS asking
    // to dock at it.
    Game *game = Game::getCurrent();
    destinations[index].location = game->targetFor(loc, true);
}

bool Craft::engageAutopilot()
{
    bool has_supply_pod{false};
    for (int pod_idx = 0; pod_idx < max_pods; ++pod_idx)
    {
        if (pods[pod_idx].type == PT_SUPPLY)
        {
            has_supply_pod = true;
            break;
        }
    }
    if (!has_supply_pod)
    {
        TraceLog(LOG_DEBUG, "Autopilot: Cannot engage autopilot on %s as no supply pod fitted", name);
        return false;
    }

    // The autopilot moves cargo, so it wants to dock at both ends. Upgrade any endpoint
    // that names a bare region to the station inside it, if there is one.
    Game *game = Game::getCurrent();
    for (int i = 0; i < MAX_DESTINATIONS; ++i)
    {
        Location *target = destinations[i].location;
        if (target && !target->isFacility())
        {
            destinations[i].location = game->targetFor(target, target->inOrbit());
        }
    }

    // launch if docked
    if (docked())
    {
        launch();
    }

    autopilot->state = AS_ON;
    return true;
}

void Craft::disengageAutopilot()
{
    autopilot->state = AS_OFF;
}