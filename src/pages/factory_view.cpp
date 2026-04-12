#include "pages/factory_view.h"
#include "state/game.h"
#include "assets/ui_elements.h"
#include "pages/overlay.h"

void FactoryView::activate()
{
    factory = nullptr;
    // set current stores, if any
    auto f = Game::getCurrent()->getCurrentFacility();
    if (f)
    {
        factory = f->factory.get();
    }
}

void FactoryView::input()
{
    // no input handling for now, but could add some interactive elements here later
}

void FactoryView::render()
{
    BasePage::render();

    // list buildables

    if (!factory)
    {
        return;
    }

    auto game{Game::getCurrent()};
    int y = 100;
    auto &overlay{Overlay::getInstance()};
    for (auto &item : game->items)
    {
        if (item.researched && factory->canBuild(item.id))
        {
            // can we build it here? tech level, orbital requirement
            if (overlay.renderButton(Rectangle{1000, (float)y, 100, 20}, item.name.c_str(), item.description.c_str(), WHITE))
            {
                // trigger production
            }
            y += 20;
        }
    }
}