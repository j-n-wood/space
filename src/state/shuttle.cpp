#include "state/shuttle.h"
#include "state/game.h"

void Shuttle::update()
{
    // state transitions

    // timed states
    if (state_timer > 0)
    {
        if (--state_timer == 0)
        {
            // done
            switch (state)
            {
            case CS_SURFACE_WORK:
                state = CS_SURFACE_DOCKED;
                break;
            case CS_ASCENDING:
                state = CS_ORBIT;
                break;
            case CS_ORBIT_WORK:
                state = CS_ORBIT;
                break;
            case CS_DESCENDING:
                // is there a facility to land at?
                if (Game::getCurrent()->resourceFacilityAt(location))
                {
                    state = CS_SURFACE_DOCKED;
                }
                else
                {
                    state = CS_SURFACE;
                }
                break;
            }
        }

    } // timed state
    else
    {
        // single tick states
        switch (state)
        {
        case CS_SURFACE_LAUNCH:
            state = CS_ASCENDING;
            state_timer = 3; // variable?
            break;
        case CS_ORBIT_LAUNCH:
            state = CS_ORBIT;
            break;
        case CS_ORBIT_DOCKING:
            state = CS_ORBIT_DOCKED;
            break;
        default:
            break;
        }
    }
}