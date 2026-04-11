#pragma once

#include "state/facility.h"
#include "state/stores.h"

class Location;

// surface facility, has derricks, draws from local resources to stores
class ResourceFacility : public Facility
{
public:
    uint32_t num_derricks;

    explicit ResourceFacility(Location *l) : Facility{l}, num_derricks{0}
    {
    }

    virtual void update() override;
};

typedef std::vector<std::unique_ptr<ResourceFacility>> Bases;