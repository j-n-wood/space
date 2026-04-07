#include "pages/overlay.h"
#include "state/game.h"
#include "pages/pages.h"

extern "C" {
    #include "raylib.h"
}

void Overlay::render() {
    // common status indicators, hover text, etc. that should be drawn on top of all pages can be rendered here
    // renders after the current page is rendered, so will appear on top of page content
    
    DrawText((Game::getInstance()).getCurrentSystem()->name.c_str(), 10, 10, 20, WHITE);    
    DrawText((PageManager::getInstance()).getCurrentPage()->title.c_str(), 100, 10, 20, WHITE);
}