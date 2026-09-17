#include "state/ios.h"
#include "state/game.h"

void IOS::update(float delta)
{
    // state transitions

    // timed states
    if (state_timer > 0.0f)
    {
        state_timer -= delta;
        if (state_timer <= 0)
        {
            state_timer = 0.0f;
            // Capture before overwriting: the switch below needs the state that just
            // EXPIRED, not the one we are moving to. Reading `state` after assigning
            // CS_IDLE made every arm below unreachable.
            const CraftState expiring = state;
            state = CS_IDLE; // most states come to rest; the arms below adjust position
            switch (expiring)
            {
            case CS_WORKING:
                if (docked())
                {
                    onDockWorkComplete();
                }
                break;
            case CS_LAUNCHING:
                break;
            case CS_DOCKING:
                onDocked();
                Game::getCurrent()->onSpacecraftDocked(this);
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

    Craft::update(delta);
}