#include "state/shuttle.h"
#include "state/game.h"

Shuttle::Shuttle(CraftState cs, uint8_t mp, Location *loc) : Craft(cs, mp, loc)
{
    type = CT_SHUTTLE;
}

void Shuttle::update(float delta)
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
                // onDocked(); // TODO - immediately dock if you completed a faciltity?
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
                break;
            default:
                break;
            }
        }
    } // timed state

    Craft::update(delta);
}