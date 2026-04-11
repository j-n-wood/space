#pragma once

class Stores;

class Factory
{
    Stores *stores;

public:
    explicit Factory(Stores *s) : stores{s} {}

    void update(); // advance time one tick
};