#pragma once

// Shared helpers for tests that drive the simulation.

#include <cmath>
#include <cstdint>

#include "../include/state/game.h"
#include "../include/state/location.h"
#include "../include/state/orbital.h"

// Advance the simulation by `seconds` of game time, as whole fixed ticks. The duration is
// rounded to the nearest tick, so 0.05s runs 3 ticks (~0.047s) and 1.0s runs exactly 64.
inline void runFor(Game *game, double seconds)
{
    for (int64_t n = std::llround(seconds / GAME_SIM_TICK); n > 0; --n)
    {
        game->tick();
    }
}

// initial.db holds the starting world; player test facilities are built in code, as
// buildTestData does in the game. Returns the operational orbital at `body`, creating
// it if the data has none.
inline Orbital *ensureOrbital(Game *game, Location *body)
{
    if (!game || !body)
    {
        return nullptr;
    }
    Orbital *orbital = game->orbitalAt(body);
    if (!orbital)
    {
        orbital = game->createOrbital(body);
    }
    if (orbital)
    {
        orbital->operational = true;
    }
    return orbital;
}
