#include "state/orbital.h"
#include "state/game.h"

Orbital::Orbital(Location *l) : Facility{l}
{
    Game::getCurrent()->createFactory(this);
}

void Orbital::update()
{
    // factory updated independently
}