#include "pages/pages.h"
#include "pages/resources.h"
#include "assets/ui_elements.h"

extern "C" {
    #include "raylib.h"
}

void Resources::render() {
    BasePage::render();

    DrawText("Resources", 10, 10, 20, WHITE);

    // controls
    DrawTexturePro(*TextureManager::getInstance().getTexture(TEXTURE_UI_BUTTONS), uiElementSources[UI_CONTROLS], BasePage::sideBarDest, (Vector2){0, 0}, 0.f, WHITE);
}

void Resources::input() {
    // no input handling for now, but could add some interactive elements here later
}