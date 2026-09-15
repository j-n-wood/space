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

    explicit ResourceFacility(Location *l, SublocationType s = SLOC_SURFACE)
        : ResourceFacility(l, LOCATION_TYPE_RESOURCE_FACILITY, s) {}
    ~ResourceFacility();

    virtual void update() override;
    bool isEarthCity() const { return type == LOCATION_TYPE_EARTH_CITY; }

protected:
    // for EarthCity, which is a resource facility of a different kind
    ResourceFacility(Location *l, LocationType t, SublocationType s);
};

// Non-owning: Game::locations owns every location, facilities included.
typedef std::vector<ResourceFacility *> Bases;