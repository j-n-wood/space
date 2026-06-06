#include "state/earth_city.h"
#include "state/research_facility.h"
#include "state/training_facility.h"

EarthCity::EarthCity(Location *l) : ResourceFacility{l}
{
    sublocation = SLOC_EARTH_CITY;
    training_facility = std::make_unique<TrainingFacility>();
    operational = true; // start operational, as we don't have construction progress implemented yet
    construction_progress = 1;
}

EarthCity::~EarthCity() {}