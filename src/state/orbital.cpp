#include "state/orbital.h"

Orbital::Orbital(Location *l) : Facility{l}, factory{&stores}
{
}

void Orbital::update()
{
    // factory updated independently
}