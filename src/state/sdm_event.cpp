#include "state/sdm_event.h"
#include "state/facility.h"
#include "state/game.h"

bool SDMEvent::cancelled()
{
    return !facility || !facility->sdm_active || facility->destroyed;
}

bool SDMEvent::update(double delta)
{
    // check for cancellation
    if (cancelled())
    {
        return false; // cancelled
    }
    time_remaining -= delta;
    if (time_remaining <= 0.0)
    {
        onComplete();
        return false; // completed
    }
    return true; // still active
}

void SDMEvent::onComplete()
{
    if (!cancelled())
    {
        // destroy facility
        Game::getCurrent()->destroyFacility(facility);
    }
}