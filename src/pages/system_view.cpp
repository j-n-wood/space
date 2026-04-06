#include <cstdlib>

#include "pages/pages.h"
#include "pages/system_view.h"
#include "assets/ui_elements.h"

extern "C" {
    #include "raylib.h"
    #include "raygui/raygui.h"
}

void SystemView::render() {    
    BasePage::render();

    DrawTexturePro(*TextureManager::getInstance().getTexture(TEXTURE_UI_BUTTONS), uiElementSources[UI_CONTROLS], BasePage::sideBarDest, (Vector2){0, 0}, 0.f, WHITE);

    DrawText("System View", 10, 10, 20, WHITE);
    orrery->render();

    UITransparentButtonState transparentButtonState; // RAII helper to set transparent button styles for the duration of this render function
    // EC button
    if (GuiButton((Rectangle){ 0, 1024 - 17*4, 51*4, 17 * 4 }, "")) {
        // TODO - switch to earth city page
        TraceLog(LOG_INFO, "ButtonClick");
    }
}

void SystemView::input() {
    Vector2 mouseWheel = GetMouseWheelMoveV();
    if (mouseWheel.y != 0)
    {
        orrery->scale += mouseWheel.y * 0.1f;
        if (orrery->scale < 0.1f) orrery->scale = 0.1f;
    }
} 