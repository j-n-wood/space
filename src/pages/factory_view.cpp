#include "pages/factory_view.h"
#include "state/game.h"
#include "assets/ui_elements.h"

void FactoryView::activate()
{
    factory = nullptr;
    // set current stores, if any
    auto f = Game::getCurrent()->getCurrentFacility();
    if (f)
    {
        factory = f->factory;
    }
}

void FactoryView::input()
{
    // no input handling for now, but could add some interactive elements here later
}

void FactoryView::render()
{
    BasePage::render();
}