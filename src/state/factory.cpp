#include <algorithm> // std::rotate vector

#include "state/factory.h"
#include "state/game.h"

QueueItem::QueueItem(const int id, bool r) : item_id{-1}, progress{0}
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

void Factory::update()
{
    // progress on current queue, if any
    // update by one tick
    if (!queue.empty())
    {
        auto &queueItem{queue[0]};

        if (!queueItem.progress)
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
                for (auto &req : item.requirements)
                {
                    stores->resources[req.resource] -= req.amount;
                }
            }
        }

        if (++queueItem.progress >= queueItem.build_time)
        {
            ++stores->items[queueItem.item_id];
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
    if (item_id >= game->items.size())
    {
        return false; // wtf?
    }
    auto &item{game->items[item_id]};

    if (item.orbital && (!this->is_orbital))
    {
        return false; // orbital only
    }

    if (item.tech_level > this->tech_level)
    {
        return false; // too hard
    }

    return true;
}