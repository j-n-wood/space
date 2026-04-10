#include "pages/overlay.h"
#include "state/game.h"
#include "pages/pages.h"

extern "C"
{
#include "raygui/raygui.h"
}

Overlay::Overlay() : currentToolTip(nullptr)
{
    // constructor can be used to initialise any state the overlay needs to track
}

void Overlay::start()
{
    // any setup that needs to be done when the overlay is first created can be done here
    currentToolTip = nullptr; // reset tooltip when starting the overlay
}

int Overlay::renderButton(const Rectangle &buttonRect, const char *buttonText, const char *toolTip, const Color &color)
{
    // Implementation for rendering a button with hover text
    // Check if mouse is hovering over the button
    if ((toolTip != nullptr) && (CheckCollisionPointRec(GetMousePosition(), buttonRect)))
    {
        setCurrentToolTip(toolTip); // set the current tooltip to be displayed by the overlay
    }
    return GuiButton(buttonRect, buttonText);
}

void Overlay::render()
{
    // common status indicators, hover text, etc. that should be drawn on top of all pages can be rendered here
    // renders after the current page is rendered, so will appear on top of page content

    Game &game{Game::getInstance()};

    DrawText(game.getCurrentSystem()->name.c_str(), 10, 10, 20, WHITE);
    if (auto location = game.getCurrentLocation())
    {
        DrawText(location->name.c_str(), 100, 10, 20, WHITE);
    }
    DrawText((PageManager::getInstance()).getCurrentPage()->title.c_str(), 200, 10, 20, WHITE);

    if (currentToolTip)
    {
        auto mp = GetMousePosition();
        DrawText(currentToolTip, mp.x + 12, mp.y + 8, 20, WHITE);
    }
}