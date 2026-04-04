#include <cstdlib>

#include "pages/pages.h"
#include "pages/system_view.h"

extern "C" {
    #include "raylib.h"
}

void SystemView::render() {
    ClearBackground(BLACK);
    DrawText("System View", 10, 10, 20, WHITE);
    orrery->render();
}