#include "state/game.h"

Game::Game() : currentSystem(nullptr) {
    for (int i = 0; i < GAME_MAX_SYSTEMS; i++) {
        systems[i] = nullptr;
    }
}

Game::~Game() {
   
}