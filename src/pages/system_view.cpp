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

void SystemView::activate(ViewState &viewState)
{
    // attach to current system
    if (System *currentSystem = PageManager::getInstance().viewState.getCurrentSystem())
    {
        setSystem(currentSystem);
    }
}

void SystemView::render()
{
    BasePage::render();

    orrery->render();
}

void SystemView::input()
{
    orrery->input();
}
