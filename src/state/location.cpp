#include "state/location.h"

Location::Location(System* s, const char* n, LocationType t) : 
    system(s),
    name(n),
    type(t)
{
}