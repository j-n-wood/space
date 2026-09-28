#include "pages/research.h"
#include "state/game.h"

extern "C"
{
#include "raylib.h"
#include "raygui/raygui.h"
}

void ResearchView::activate(ViewState &viewState)
{
    facility = viewState.getCurrentResearchFacility();
}

void ResearchView::input()
{
}

void ResearchView::render()
{
    BasePage::render();

    // location resources
    listResearch();

    // display crew
    if (facility->crew)
    {
        char crew_status[128];
        DrawText(facility->crew->description(crew_status, sizeof crew_status), 270, 770, 20, BLUE);
        snprintf(crew_status, sizeof crew_status, "Researchers: %d", facility->crew->size);
        DrawText(crew_status, 270, 790, 20, BLUE);
    }
}

void ResearchView::listResearch()
{
    if (!facility)
    {
        return;
    }
    Vector2 cursor{400, 64};

    auto game{Game::getCurrent()};

    if (facility->current_project == -1)
    {
        DrawText("No active research project.", cursor.x, cursor.y, 20, WHITE);
    }
    else
    {
        auto &topic = game->researchTopics[facility->current_project];
        float progress = topic.progress / topic.requiredTime * 100.0f;
        char buf[256];
        std::snprintf(buf, sizeof buf, "%s: %.1f %% ", topic.description, progress);
        DrawText(buf, cursor.x, cursor.y, 20, WHITE);
    }

    // iterate and list available research topics
    cursor.x = 1000;
    cursor.y = 64;

    for (auto &topic : game->researchTopics)
    {
        if (topic.available)
        {
            if (GuiButton((Rectangle){cursor.x, cursor.y, 200, 32}, topic.name))
            {
                // start researching this topic
                facility->current_project = topic.id;
            }
            cursor.y += 32;
        }
    }
}
