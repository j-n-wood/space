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
        auto game = Game::getCurrent();
        auto &topic = game->researchTopics[current_project];
        float progress = topic.progress;
        float project_time = topic.requiredTime;
        if (progress + delta >= project_time)
        {
            // complete project
            topic.progress = topic.requiredTime;
            topic.available = false; // no longer available to research

            // if any unlocks
            if (!topic.unlocksItems.empty())
            {
                for (auto &itemId : topic.unlocksItems)
                {
                    game->items[itemId].researched = true;
                }
            }
            if (!topic.unlocksTopics.empty())
            {
                for (auto &topicId : topic.unlocksTopics)
                {
                    game->researchTopics[topicId].available = true;
                }
            }

            // move to next project if desired - for now just stop
            current_project = -1;

            // if we had a queue, we can flow over the remaining delta to the next project here
        }
        else
        {
            // still researching
            topic.progress += delta;
        }
    }
}