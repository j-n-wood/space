#include "state/craft_under_attack_event.h"
#include "state/craft.h"
#include "state/game.h"

extern "C"
{
#include "raylib.h"
}

CraftUnderAttackEvent::CraftUnderAttackEvent(double duration, Craft *att, Craft *target)
    : CraftRealtimeEvent(duration, target), time_until_damage(4.0), initial_location(target->location), attacker(att)
{
    type = RealtimeEventType::Craft_Under_Attack;

    TraceLog(LOG_INFO, "Warship %s attack on target craft at location %s", att->name, target->location->name);
}

bool CraftUnderAttackEvent::cancelled()
{
    bool cancelled = (craft->location != initial_location) || (craft->destroyed);
    if (cancelled)
    {
        attacker->engagement = nullptr;
        attacker = nullptr;
    }
    return cancelled;
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
            TraceLog(LOG_INFO, "Warship %s deals damage to target craft at location %s", attacker->name, craft->location->name);
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
        TraceLog(LOG_INFO, "Warship %s attack on target craft at location %s completed", attacker->name, craft->location->name);
        // destroy craft
        Game::getCurrent()->destroyCraft(craft);
    }
    craft = nullptr;
}