#include "state/orbital.h"
#include "state/game.h"

Orbital::Orbital(Location *l) : Facility{l}
{
    sublocation = SLOC_ORBIT; // default, but make sure
    Game::getCurrent()->createFactory(this);
}

void Orbital::update()
{
    // factory updated independently
}