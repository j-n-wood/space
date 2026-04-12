#pragma once

class Stores;

class Factory
{
    Stores *stores;

    bool is_orbital;
    int tech_level;

public:
    explicit Factory(Stores *s) : stores{s}, is_orbital{true}, tech_level{1} {}

    void update(); // advance time one tick

    bool canBuild(const int item_id) const;
};