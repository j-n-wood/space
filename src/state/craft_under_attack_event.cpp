#include "state/craft_under_attack_event.h"
#include "state/craft.h"
#include "state/game.h"

CraftUnderAttackEvent::CraftUnderAttackEvent(double duration, Craft *craft)
    : CraftRealtimeEvent(duration, craft), time_until_damage(4.0), initial_location(craft->location) {}

bool CraftUnderAttackEvent::cancelled()
{
    return craft->location != initial_location;
}

bool CraftUnderAttackEvent::update(double delta)
{
    // check for cancellation
    // if craft location changes, cancel the event
    if (cancelled())
    {
        return false; // cancelled
    }

    if (time_until_damage > 0.0)
    {
        time_until_damage -= delta;
        if (time_until_damage <= 0.0)
        {
            // apply damage to craft
            craft->applyDamage();
        }
    }

    time_remaining -= delta;
    if (time_remaining <= 0.0)
    {
        onComplete();
        return false; // completed
    }
    return true; // still active
}

void CraftUnderAttackEvent::onComplete()
{
    if (!cancelled())
    {
        // destroy craft
        Game::getCurrent()->queueDestroyedCraft(craft);
    }
    craft = nullptr;
}