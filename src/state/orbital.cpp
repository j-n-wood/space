#include "state/orbital.h"

Orbital::Orbital(Location *l) : Facility{l}
{
    factory = std::make_unique<Factory>(&stores);
}

void Orbital::update()
{
    // factory updated independently
}