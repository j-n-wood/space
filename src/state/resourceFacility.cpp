#include "state/resourceFacility.h"
#include "state/location.h"
#include "state/research_facility.h"
#include "state/training_facility.h"

ResourceFacility::ResourceFacility(Location *l, LocationType t)
    : Facility{l, t}, num_derricks{0}
{
}

ResourceFacility::~ResourceFacility() {};

void ResourceFacility::update()
{
    Facility::update();

    // Collect from the BODY. Availability is loaded onto bodies, while `primary` is the
    // surface region the facility sits in -- which has none of its own.
    Location *source = body();
    if (source && num_derricks && (damage < 1))
    {
        // iterate available resources
        // TODO seam limits, survey time, collection rate
        for (int idx = 0; idx < ResourceType::Count; ++idx)
        {
            if (source->resources.availability[idx])
            {
                stores.resources[idx] += num_derricks;
            }
        }
    }
}