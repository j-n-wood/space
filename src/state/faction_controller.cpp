#include "state/faction_controller.h"
#include "state/game.h"

void FactionController::update()
{
    Game *game{Game::getCurrent()};
    if (!game->factions[FACTION_METHANOID].hostile)
    {
        return;
    }

    // produce drones based on orbitals that are active

    // iterate orbitals, produce drones, send overproduction to one orbital per system
    // when sufficient are available, spawn a warship and send to engage player orbitals in the same
    // system. If no such orbital exists, send interstellar.
}