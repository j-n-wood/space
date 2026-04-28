#include <cstdio>
#include "state/craft.h"
#include "state/game.h"
#include "state/resources.h"
#include "state/autopilot.h"

const char *PodTypeName[PT_COUNT] = {
    "EMPTY",
    "TOOL",
    "SUPPLY",
    "CRYO"};

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
    // TODO location can be missing
    const char *location_name = location ? location->name : "Space";

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

void Craft::setDestination(const uint8_t index, Location *loc)
{
    if (index >= MAX_DESTINATIONS)
    {
        return;
    }
    destinations[index].location = loc;
    // set to dock if autopilot is engaged
    destinations[index].docked = (autopilot->state >= AS_ON);
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