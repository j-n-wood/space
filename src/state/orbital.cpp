#include "state/orbital.h"
#include "state/game.h"

Orbital::Orbital(Location *l) : Facility{l}
{
    sublocation = SLOC_ORBIT;                // default, but make sure
    Game::getCurrent()->createFactory(this); // perhaps we should avoid the singleton reference here
}

void Orbital::update()
{
    // factory updated independently
}