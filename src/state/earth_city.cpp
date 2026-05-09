#include "state/earth_city.h"
#include "state/research_facility.h"
#include "state/training_facility.h"
#include "state/game.h"

EarthCity::EarthCity(Location *l) : ResourceFacility{l}
{
    auto game = Game::getCurrent();
    game->createResearchFacility(this);
    training_facility = std::make_unique<TrainingFacility>();
}

EarthCity::~EarthCity() {}