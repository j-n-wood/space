#pragma once

#include "state/item.h"
#include "state/string_caps.h"
#include <vector>

class ResearchTopic
{
public:
    int id;
    char name[NAME_MAX_LEN];
    char description[DESC_MAX_LEN];
    float requiredTime;
    float progress;
    bool available;
    std::vector<ItemType> unlocksItems;
    std::vector<int> unlocksTopics; // referenced by id, which shall also be index

    ResearchTopic() : id{-1}, name{""}, description{""}, requiredTime{0}, progress{0}, available{false} {};
};