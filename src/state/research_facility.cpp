#include "state/research_facility.h"
#include "state/game.h"

// research
// current project is index into game->items, which have a research time

void ResearchFacility::update(float delta)
{
    // research progress

    // if enough to complete project, mark as researched and move to next project if desired
    // currently no queue, but prepare for it

    if (current_project != -1)
    {
        if (delta >= project_time)
        {
            // complete project
            // mark item as researched
            Game::getCurrent()->items[current_project].researched = true;

            // move to next project if desired - for now just stop
            current_project = -1;
            project_time = 0;

            // if we had a queue, we can flow over the remaining delta to the next project here
        }
        else
        {
            // still researching
            project_time -= delta;
        }
    }
}