#include "state/earth_city.h"
#include "state/research_facility.h"
#include "state/training_facility.h"

EarthCity::EarthCity(Location *l) : ResourceFacility{l}
{
    training_facility = std::make_unique<TrainingFacility>();
}

EarthCity::~EarthCity() {}