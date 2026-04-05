#include "pages/pages.h"
#include "pages/earth_city.h"
#include "assets/ui_elements.h"

extern "C" {
    #include "raylib.h"
}

void EarthCity::render() {
    BasePage::render();

    DrawText("Earth City", 10, 10, 20, WHITE);

    // controls
    DrawTexturePro(*TextureManager::getInstance().getTexture(TEXTURE_UI_BUTTONS), uiElementSources[UI_EARTH_CITY_CONTROLS], BasePage::sideBarDest, (Vector2){0, 0}, 0.f, WHITE);
}

void EarthCity::input() {
    // no input handling for now, but could add some interactive elements here later
}