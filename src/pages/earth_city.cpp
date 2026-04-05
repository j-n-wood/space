#include "pages/pages.h"
#include "pages/earth_city.h"

extern "C" {
    #include "raylib.h"
}

void EarthCity::render() {
    BasePage::render();

    DrawText("Earth City", 10, 10, 20, WHITE);
}

void EarthCity::input() {
    // no input handling for now, but could add some interactive elements here later
}