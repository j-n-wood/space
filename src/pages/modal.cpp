#include "pages/modal.h"

#include "pages/overlay.h"
#include "pages/pages.h"
#include "assets/ui_elements.h"

void Modal::activate()
{
    visible = true;
    PageManager::getInstance().setModal(true);
}

void Modal::deactivate()
{
    visible = false;
    PageManager::getInstance().setModal(false);
}

// scope modal state
void Modal::renderModal()
{
    if (!visible)
    {
        return;
    }

    ControlLockToggle lock(false); // unlock controls for this modal
    render();
}

void Modal::render()
{
    // add a dimmed background to make it clear this is an overlay
    DrawRectangle(left, top, GetScreenWidth() - 2 * left, GetScreenHeight() - 2 * top, (Color){0, 0, 0, 128});
    DrawRectangleLines(left, top, GetScreenWidth() - 2 * left, GetScreenHeight() - 2 * top, (Color){200, 180, 170, 255});
}