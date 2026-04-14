#include "pages/factory_view.h"
#include "state/game.h"
#include "assets/ui_elements.h"
#include "pages/overlay.h"
#include "pages/pages.h"
#include <string>

void FactoryView::activate(ViewState &viewState)
{
    factory = nullptr;
    // set current stores, if any
    auto f = viewState.getCurrentFacility();
    if (f)
    {
        factory = f->factory.get();
    }
}

void FactoryView::input()
{
    // no input handling for now, but could add some interactive elements here later
}

struct hoverState
{
    Factory *factory;
    Item *item;

    explicit hoverState(Factory *f) : factory{f}, item{nullptr} {}
};

void renderPlanHover(void *state)
{
    hoverState *hs = static_cast<hoverState *>(state);
    // set overlay tooltip
    Overlay::getInstance().setCurrentToolTip(hs->item->description.c_str());

    Vector2 reqsCursor{240, 200};

    for (auto &req : hs->item->requirements)
    {
        // list required resources
        DrawText(ResourceName[req.resource], reqsCursor.x, reqsCursor.y, 20, YELLOW);
        std::string amount = std::to_string(req.amount);
        Color textColour = YELLOW;
        if (hs->factory->stores->resources[req.resource] < req.amount)
        {
            textColour = RED;
        }
        DrawText(amount.c_str(), reqsCursor.x + 100, reqsCursor.y, 20, textColour);
        reqsCursor.y += 20;
    }
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
    hoverState hoverState{factory};
    for (auto &item : game->items)
    {
        if (item.researched && factory->canBuild(item.id))
        {
            // can we build it here? tech level, orbital requirement
            hoverState.item = &item;
            if (overlay.renderButtonHover(Rectangle{1000, (float)y, 100, 20}, item.name.c_str(), WHITE, renderPlanHover, &hoverState))
            {
                // trigger production
                factory->queueItem(item.id);
            }
            y += 20;
        }
    }

    // list queue
    y = 500;
    for (size_t idx = 0, imax = factory->queue.size(); idx < imax; ++idx)
    {
        auto &q{factory->queue[idx]};
        if (GuiButton(Rectangle{1000, (float)y, 100, 20}, game->items[q.item_id].name.c_str()))
        {
            // drop from queue
            factory->dropQueueItem((int)idx);
        }
        y += 20;
    }
}