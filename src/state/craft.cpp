#include <cstdio>
#include <cmath>
#include "state/craft.h"
#include "state/game.h"
#include "state/resources.h"
#include "state/autopilot.h"

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

Craft::Craft(CraftState cs, uint8_t mp, Location *loc) : state{cs}, state_timer{0.0f}, max_pods{mp}, drive{false}, location{loc}, destination_index{0}, autopilot{std::make_unique<Autopilot>()}
{
    name[0] = '\0';
};

Craft::~Craft()
{
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
    // update autopilot if fitted
    if (drive)
    {
        // update autopilot logic here
        autopilot->update(this, delta);
    }

    // working states
    if ((state == CS_SURFACE_DOCK_WORK) && (pods[0].contentType == ItemType::Bandaid)) // TODO
    {
        // reduce damage level as time passes
        ResourceFacility *rf = Game::getCurrent()->resourceFacilityAt(location);
        if (rf)
        {
            rf->damage = std::max(0.0f, rf->damage - (delta * 5.0f)); // repair rate constant, TODO move to game state
            if (rf->damage == 0)
            {
                TraceLog(LOG_INFO, "Completed repairing facility at location %s", location->name);
                setTimedState(CS_SURFACE_DOCKED, 0); // back to docked state when repair complete
            }
        }
    }
}

Craft &Craft::arriveAtLocation()
{
    auto &current_dest{destinations[destination_index]};
    location = current_dest.location;
    if (atEndpoint())
    {
        TraceLog(LOG_INFO, "Arrived at destination: %s", location ? location->name : "Space");
        nextEndpoint();
    }
    return *this;
}

void Craft::onDocked()
{
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
    const char *location_name = location ? location->name : "Space"; // TODO location cannot be empty now

    switch (state)
    {
    case CS_SURFACE: // surface no dock
        std::snprintf(status, len, "On surface of %s", location_name);
        break;
    case CS_SURFACE_DOCKED:
        std::snprintf(status, len, "Docked at %s station", location_name);
        break;
    case CS_SURFACE_WORK:
        std::snprintf(status, len, "Working on %s surface", location_name);
        break;
    case CS_SURFACE_DOCK_WORK:
        std::snprintf(status, len, "Working at %s station", location_name);
        break;
    case CS_SURFACE_LAUNCH:
        std::snprintf(status, len, "Launching from %s", location_name);
        break;
    case CS_ASCENDING:
        std::snprintf(status, len, "Ascending from %s", location_name);
        break;
    case CS_ORBIT:
        std::snprintf(status, len, "Orbiting %s", location_name);
        break;
    case CS_ORBIT_DOCKING:
        std::snprintf(status, len, "Docking with %s orbital", location_name);
        break;
    case CS_ORBIT_DOCKED:
        std::snprintf(status, len, "Docked at %s orbital", location_name);
        break;
    case CS_ORBIT_DOCK_WORK:
        std::snprintf(status, len, "Working at %s orbital", location_name);
        break;
    case CS_ORBIT_WORK:
        std::snprintf(status, len, "Working in %s orbit", location_name);
        break;
    case CS_ORBIT_LAUNCH:
        std::snprintf(status, len, "Launching from %s orbital", location_name);
        break;
    case CS_DESCENDING:
        std::snprintf(status, len, "Descending to %s", location_name);
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
    if (destinations[destination_index].location)
    {
        // cannot engage drive if docked
        if (state == CS_SURFACE_DOCKED || state == CS_ORBIT_DOCKED)
        {
            TraceLog(LOG_WARNING, "Cannot engage drive while docked");
            return *this;
        }

        auto source = location;
        auto destination = destinations[destination_index].location;

        // make up a transit time based on location distance
        Game *game = Game::getCurrent();

        total_state_timer = game->transitTimeCalculator->calculateTransitTime(source, destination);
        state_timer = total_state_timer;
        TraceLog(LOG_INFO, "Engaging drive from %s to %s, transit time %.1f seconds", source ? source->name : "Space", destination->name, total_state_timer);

        state = CS_TRANSIT;
        location = location->system->space; // space location for system
    }
    return *this;
}

Craft &Craft::disengageDrive()
{
    if (state == CS_TRANSIT)
    {
        TraceLog(LOG_INFO, "Disengaging drive");
    }
    return *this;
}

void Craft::setDestination(const uint8_t index, Location *loc)
{
    if (index >= MAX_DESTINATIONS)
    {
        return;
    }
    destinations[index].location = loc;
    // set to dock if autopilot is engaged

    Game *game = Game::getCurrent();
    Orbital *orbital = game->orbitalAt(loc); // set docking target if have an orbital station at the destination

    destinations[index].docked = (autopilot->state >= AS_ON) && (orbital != nullptr);
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

    // force destination endpoints to be docked
    for (int i = 0; i < MAX_DESTINATIONS; ++i)
    {
        if (destinations[i].location)
        {
            destinations[i].docked = true;
        }
    }

    // launch if docked
    if (state == CS_SURFACE_DOCKED || state == CS_ORBIT_DOCKED)
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