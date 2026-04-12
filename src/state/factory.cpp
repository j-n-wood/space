#include "state/factory.h"
#include "state/game.h"

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