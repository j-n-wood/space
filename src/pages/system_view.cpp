#include <cstdlib>

#include "pages/pages.h"
#include "pages/system_view.h"
#include "assets/ui_elements.h"

extern "C" {
    #include "raylib.h"
}

void SystemView::render() {    
    BasePage::render();

    DrawTexturePro(*TextureManager::getInstance().getTexture(TEXTURE_UI_BUTTONS), uiElementSources[UI_CONTROLS], BasePage::sideBarDest, (Vector2){0, 0}, 0.f, WHITE);

    DrawText("System View", 10, 10, 20, WHITE);
    orrery->render();
}

void SystemView::input() {
    Vector2 mouseWheel = GetMouseWheelMoveV();
    if (mouseWheel.y != 0)
    {
        orrery->scale += mouseWheel.y * 0.1f;
        if (orrery->scale < 0.1f) orrery->scale = 0.1f;
    }
} 