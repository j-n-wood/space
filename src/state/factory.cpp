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