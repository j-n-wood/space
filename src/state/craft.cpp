#include <cstdio>
#include "state/craft.h"
#include "state/game.h"
#include "state/resources.h"

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
        autopilot.update(this, delta);
    }
}