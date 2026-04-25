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
            // done
            switch (state)
            {
            case CS_ORBIT_WORK:
                state = CS_ORBIT;
                break;
            case CS_ORBIT_LAUNCH:
                state = CS_ORBIT;
                break;
            case CS_ORBIT_DOCKING:
                state = CS_ORBIT_DOCKED;
                break;
            case CS_TRANSIT:
                arrive();
                state = CS_ORBIT;
                Game::getCurrent()->onSpacecraftArrival(this);
                break;
            default:
                break;
            }
        }
    } // timed state

    Craft::update(delta);
}