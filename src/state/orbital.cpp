#include "state/orbital.h"

Orbital::Orbital(Location *l) : Facility{l}
{
    sublocation = SLOC_ORBIT; // default, but make sure
}

void Orbital::update()
{
    // factory updated independently
}