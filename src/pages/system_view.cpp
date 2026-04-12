#include <cstdlib>

#include "pages/pages.h"
#include "pages/system_view.h"
#include "assets/ui_elements.h"
#include "pages/pages.h"
#include "state/game.h"

extern "C"
{
#include "raylib.h"
#include "raygui/raygui.h"
}

void SystemView::activate()
{
    // attach to current system
    System *currentSystem = Game::getCurrent()->getCurrentSystem();
    if (currentSystem)
    {
        setSystem(currentSystem);
    }
}

void SystemView::render()
{
    BasePage::render();

    // DrawText("System View", 10, 10, 20, WHITE);
    orrery->render();

    UITransparentButtonState transparentButtonState;
    // EC button
    if (GuiButton((Rectangle){0, 1024 - 17 * 4, 51 * 4, 17 * 4}, ""))
    {
        PageManager::getInstance().switchToPage(PAGE_EARTH_CITY);
    }
}

void SystemView::input()
{
    Vector2 mouseWheel = GetMouseWheelMoveV();
    if (mouseWheel.y != 0)
    {
        orrery->scale += mouseWheel.y * 0.1f;
        if (orrery->scale < 0.1f)
            orrery->scale = 0.1f;
    }
}