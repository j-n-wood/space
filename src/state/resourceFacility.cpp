#include "state/resourceFacility.h"
#include "state/location.h"

void ResourceFacility::update()
{
    Facility::update();

    // collect resources from location
    if (location && num_derricks)
    {
        // iterate available resources
        // TODO seam limits, survey time, collection rate
        for (int idx = 0; idx < ResourceType::Count; ++idx)
        {
            if (location->resources.availability[idx])
            {
                stores.resources[idx] += num_derricks;
            }
        }
    }
}