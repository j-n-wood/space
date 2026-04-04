#include <cstdlib>

#include "pages/pages.h"
#include "pages/system_view.h"

extern "C" {
    #include "raylib.h"
}

void SystemView::render() {    
    BasePage::render();

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