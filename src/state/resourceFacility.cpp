#include "state/resourceFacility.h"
#include "state/location.h"
#include "state/research_facility.h"
#include "state/training_facility.h"

ResourceFacility::ResourceFacility(Location *l, LocationType t, SublocationType s)
    : Facility{l, t, s}, num_derricks{0}
{
}

ResourceFacility::~ResourceFacility() {};

void ResourceFacility::update()
{
    Facility::update();

    // collect resources from the body this facility sits on
    if (primary && num_derricks && (damage < 1))
    {
        // iterate available resources
        // TODO seam limits, survey time, collection rate
        for (int idx = 0; idx < ResourceType::Count; ++idx)
        {
            if (primary->resources.availability[idx])
            {
                stores.resources[idx] += num_derricks;
            }
        }
    }
}