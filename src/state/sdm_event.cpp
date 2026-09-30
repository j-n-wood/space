#include "state/sdm_event.h"
#include "state/facility.h"

bool SDMEvent::update(double delta)
{
    // check for cancellation
    if (facility && !facility->sdm_active)
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
    // destroy facility
}