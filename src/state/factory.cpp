#include <algorithm> // std::rotate vector

#include "state/factory.h"
#include "state/game.h"

QueueItem::QueueItem(const int id, bool r) : item_id{-1}, progress{0.0f}
{
    if (id < Game::getCurrent()->items.size())
    {
        item_id = id;
        progress = 0;
        build_time = Game::getCurrent()->items[id].production_time;
        repeat = r;
    }
};

void Factory::queueItem(const int item_id)
{
    QueueItem q{item_id, false};
    if (q.isValid())
    {
        queue.push_back(q); // needed to use checks on item type
    }
}

void Factory::dropQueueItem(const int index)
{
    if ((index < queue.size()) && (index >= 0))
    {
        queue.erase(queue.begin() + index);
    }
}

void Factory::repeatQueueItem(const int index, const bool r)
{
    if ((index >= 0) && (index < queue.size()))
    {
        queue[index].repeat = r;
    }
}

bool Factory::sendToStores(const int item_id)
{
    // some items will be installed at facility instead
    if (facility)
    {
        switch (item_id)
        {
        case ItemType::SDM:
            if (!facility->sdm_installed)
            {
                facility->sdm_installed = true;
                return false;
            }
            break;
        case ItemType::AOC:
            if (!aoc_installed)
            {
                aoc_installed = true;
                // send crew to barracks
                if (crew)
                {
                    facility->barracks.addCrew(crew); // TODO could fail if full
                }
                return false;
            }
            break;
        case ItemType::MTX:
            if (!facility->mtx_installed)
            {
                facility->mtx_installed = true;
                return false;
            }
            break; // surplus goes to stores like any other item
        default:
            break;
        }
    }

    if (!stores)
    {
        return false;
    }

    ++stores->items[item_id];
    return true;
}

void Factory::update()
{
    if (facility->destroyed)
    {
        return;
    }

    // if no crew, cannot do anything
    if ((!crew) && (!aoc_installed))
    {
        return;
    }

    // more experienced or automated factories work faster
    float factory_rate = 1.0f;
    if (aoc_installed)
    {
        factory_rate = 2.0f; // example: AOC doubles factory rate
    }
    else
    {
        // rank 1-4
        factory_rate = 1.0f + crew->rank * 0.125f;
    }

    // progress on current queue, if any
    // update by one tick
    if (!queue.empty())
    {
        auto &queueItem{queue[0]};

        if (!queueItem.started)
        {
            // starting, see if we can get resources
            auto &item{Game::getCurrent()->items[queueItem.item_id]};

            bool buildable = true;
            for (auto &req : item.requirements)
            {
                if (stores->resources[req.resource] < req.amount)
                {
                    buildable = false; // need feedback - hover shows
                }
            }

            if (buildable)
            {
                // consume resources
                for (auto &req : item.requirements)
                {
                    stores->resources[req.resource] -= req.amount;
                }
                queueItem.started = true;
            }
            // else wait for resources
            return; // if can't start, don't progress time
        }

        // add some crew experience
        if (crew)
        {
            crew->addExperience(1.0f); // adjust rate as needed
        }

        float progress = 1.0f * factory_rate;
        if ((queueItem.progress += progress) >= queueItem.build_time)
        {
            // if a facility feature (SDM, AOC, MTX) this activates the feature if not present.
            // otherwise fall through to stores
            sendToStores(queueItem.item_id);

            Game::getCurrent()->raiseProductionCompleteEvent(this, queueItem.item_id);

            // done!
            if (queueItem.repeat)
            {
                queueItem.progress = 0;
                if (queue.size() > 1)
                {
                    // move to back of queue
                    std::rotate(queue.begin(), queue.begin() + 1, queue.end());
                }
            }
            else
            {
                // pop from front of queue - may want to change to a list
                queue.erase(queue.begin());
            }
        }
    }
}

bool Factory::canBuild(const int item_id) const
{
    auto game{Game::getCurrent()};
    if (item_id == 0)
    {
        // nothing
        return false;
    }
    if (item_id >= game->items.size())
    {
        return false; // wtf?
    }
    auto &item{game->items[item_id]};

    if (item.orbital && (!is_orbital))
    {
        return false; // orbital only
    }

    if (item.tech_level > getTechLevel())
    {
        return false; // too hard
    }

    return true;
}

int Factory::getTechLevel() const
{
    if (!facility)
    {
        return 0; // default tech level if no facility
    }
    if (aoc_installed)
    {
        return 5;
    }
    return crew ? crew->rank : 0;
}

bool Factory::assignCrewFromFacility(Crew *c)
{
    if (!c || !facility)
    {
        TraceLog(LOG_INFO, "Cannot assign crew: invalid crew or no facility");
        return false;
    }

    if (aoc_installed)
    {
        TraceLog(LOG_INFO, "Cannot assign crew: AOC installed");
        return false;
    }

    Crew *prior_crew = crew;

    facility->barracks.removeCrew(c);
    if (prior_crew)
    {
        facility->barracks.addCrew(prior_crew);
    }

    crew = c;
    return true;
}