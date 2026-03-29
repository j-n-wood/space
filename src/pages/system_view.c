#include <stdlib.h>

#include "pages/pages.h"
#include "pages/system_view.h"

#include "raylib.h"

SystemView* createSystemView(Orrery* orrery) {
    SystemView* view = malloc(sizeof(SystemView));
    view->orrery = orrery;
    return view;
}

void destroySystemView(SystemView* view) {
    if (view == NULL) return;
    free(view);
}

void RenderSystemView(SystemView* view) {
    ClearBackground(BLACK);
    DrawText("System View", 10, 10, 20, WHITE);
    renderOrrery(view->orrery);
}