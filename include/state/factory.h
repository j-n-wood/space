#pragma once

#include "state/item.h"

#include <vector>

class Stores;
class Facility; // for crew transfer
class Crew;

class QueueItem
{
public:
    int item_id;
    int build_time;
    int progress;
    bool started;
    bool repeat;

    explicit QueueItem(const int id, bool repeat);
    inline bool isValid() const { return item_id > ItemType::None; }
};

typedef std::vector<QueueItem> FactoryQueue;

class Factory
{

public:
    bool is_orbital;

    Crew *crew;
    bool aoc_installed;

    explicit Factory(Facility *f, Stores *s) : facility{f}, stores{s}, is_orbital{true}, crew{nullptr}, aoc_installed{false} {}

    Facility *facility;
    Stores *stores;
    FactoryQueue queue;

    void update(); // advance time one tick

    bool canBuild(const int item_id) const;
    inline bool isActive() const { return !queue.empty() && queue.front().started; }

    void queueItem(const int item_id);
    void dropQueueItem(const int index);
    void repeatQueueItem(const int index, const bool r);

    int getTechLevel() const;
    bool assignCrewFromFacility(Crew *c);
};