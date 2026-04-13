#pragma once

#include <vector>

class Stores;

class QueueItem
{
public:
    int item_id;
    int build_time;
    int progress;
    bool repeat;

    explicit QueueItem(const int id, bool repeat);
    inline bool isValid() const { return item_id > -1; }
};

typedef std::vector<QueueItem> FactoryQueue;

class Factory
{
    bool is_orbital;
    int tech_level;

public:
    explicit Factory(Stores *s) : stores{s}, is_orbital{true}, tech_level{1} {}

    Stores *stores;
    FactoryQueue queue;

    void update(); // advance time one tick

    bool canBuild(const int item_id) const;

    void queueItem(const int item_id);
    void dropQueueItem(const int index);
    void repeatQueueItem(const int index, const bool r);
};