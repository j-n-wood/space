#include "state/shuttle.h"
#include "state/game.h"

Shuttle::Shuttle(CraftState cs, uint8_t mp, Location *loc) : Craft(cs, mp, loc)
{
    type = CT_SHUTTLE;
}
