#pragma once

#include "state/facility.h"
#include "state/stores.h"

class Location;
class ResearchFacility;
class TrainingFacility;

// surface facility, has derricks, draws from local resources to stores
class ResourceFacility : public Facility
{
public:
    uint32_t num_derricks;
    std::unique_ptr<ResearchFacility> research_facility;
    std::unique_ptr<TrainingFacility> training_facility;

    explicit ResourceFacility(Location *l);
    ~ResourceFacility();

    virtual void update() override;
};

typedef std::vector<std::unique_ptr<ResourceFacility>> Bases;