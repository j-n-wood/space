#include "pages/factory_view.h"
#include "state/game.h"
#include "assets/ui_elements.h"
#include "pages/overlay.h"
#include "pages/pages.h"
#include <cstdio>
#include <cmath>

Rectangle itemImageTarget = {250, 140, 256, 224};
Rectangle docImageTarget = {560, 240, 192, 192};
Rectangle grinderSource = {464, 384, 21, 9};  // leftmost part of repeated shaft, 6 pix repeat
Rectangle grinderTarget = {983, 488, 81, 54}; // where to blit the shaft

void FactoryView::activate(ViewState &viewState)
{
    factory = nullptr;
    shaft_animation = 0.0f;

    auto game{Game::getCurrent()};
    Facility *f{nullptr};
    switch (side)
    {
    case LOCATION_TYPE_ORBIT:
        f = game->orbitalAt(viewState.getCurrentBody());
        break;
    case LOCATION_TYPE_SURFACE:
        f = game->resourceFacilityAt(viewState.getCurrentBody());
        break;
    default:
        break;
    }

    if (f)
    {
        factory = f->factory.get();
        Game::getCurrent()->addEventSink(this);
    }
}

void FactoryView::deactivate()
{
    Game::getCurrent()->removeEventSink(this);
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
    auto docImageTexture = TextureManager::getInstance().getTexture(TEXTURE_ITEMS);

    hoverState *hs = static_cast<hoverState *>(state);
    // set overlay tooltip
    Overlay::getInstance().setCurrentToolTip(hs->item->description);

    Vector2 reqsCursor{240, 500};

    for (auto &req : hs->item->requirements)
    {
        // list required resources
        DrawText(ResourceName[req.resource], reqsCursor.x, reqsCursor.y, 20, YELLOW);
        char amount[16];
        std::snprintf(amount, sizeof amount, "%d", req.amount);
        Color textColour = YELLOW;
        if (hs->factory->stores->resources[req.resource] < req.amount)
        {
            textColour = RED;
        }
        DrawText(amount, reqsCursor.x + 100, reqsCursor.y, 20, textColour);
        reqsCursor.y += 20;
    }

    if (hs->item->doc_image_index > -1)
    {
        DrawTexturePro(*docImageTexture, itemImageSources[hs->item->doc_image_index], docImageTarget, (Vector2){0, 0}, 0.f, WHITE);
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
            hoverState.item = &item;
            if (overlay.renderButtonHover(Rectangle{1100, (float)y, 100, 20}, item.name, WHITE, renderPlanHover, &hoverState))
            {
                // trigger production
                factory->queueItem(item.id);
            }
            y += 20;
        }
    }

    // list queue
    y = 600;
    for (size_t idx = 0, imax = factory->queue.size(); idx < imax; ++idx)
    {
        auto &q{factory->queue[idx]};
        if (GuiButton(Rectangle{1100, (float)y, 100, 20}, game->items[q.item_id].name))
        {
            // drop from queue
            factory->dropQueueItem((int)idx);
        }
        y += 20;
    }

    // TODO share and load once
    auto docImageTexture = TextureManager::getInstance().getTexture(TEXTURE_ITEMS);

    // current queue item
    if (!factory->queue.empty())
    {
        auto &qi{factory->queue[0]};
        auto &item{game->items[qi.item_id]};

        if (item.production_image_index > -1)
        {
            // determine progress
            int imagine_index = II_PROD_CRATE; // finished
            auto frac = floor(3.0 * (float)(qi.progress) / (float)(qi.build_time));
            if (frac < 4.0)
            {
                imagine_index = (int)frac + item.production_image_index;
            }

            DrawTexturePro(*docImageTexture, itemImageSources[imagine_index], itemImageTarget, (Vector2){0, 0}, 0.f, WHITE);
        }
    }

    // blit shaft image over grinder area - always, so progress looks consistent
    Rectangle source = grinderSource;
    source.x += (int)(shaft_animation * 6.0f); // 6 pixel repeat
    DrawTexturePro(*backgroundTexture, source, grinderTarget, (Vector2){0, 0}, 0.f, WHITE);

    // report crew
    if (factory->aoc_installed)
    {
        DrawText("Automated Operations Computer", 684, 832, 20, GRAY);
    }
    else if (factory->crew)
    {
        Crew *crew = factory->crew;
        char crew_status[128];
        DrawText(crew->description(crew_status, sizeof crew_status), 684, 852, 20, YELLOW);
        std::snprintf(crew_status, sizeof crew_status, "%d engineers", crew->size);
        DrawText(crew_status, 684, 872, 20, YELLOW);

        if (factory->facility->barracks.hasSpace() && GuiButton(Rectangle{1070, 852, 100, 20}, "Send to barracks"))
        {
            factory->facility->barracks.addCrew(crew);
            factory->crew = nullptr; // remove crew from factory after sending to barracks
        }
    }

    pageLog.render();
}

void FactoryView::update(const float delta)
{
    // if we have something in production, animate the grinder shaft
    if (factory && !factory->queue.empty())
    {
        shaft_animation += delta;
        if (shaft_animation > 1.0f)
        {
            shaft_animation -= 1.0f;
        }
    }

    pageLog.update(delta);
}

void FactoryView::onProductionComplete(Factory *f, int item_id)
{
    if (f != this->factory)
    {
        return;
    }

    auto game{Game::getCurrent()};
    auto &item{game->items[item_id]};

    char log[256];
    std::snprintf(log, sizeof log, "Produced %s", item.name);
    pageLog.addLog(log);
}