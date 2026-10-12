#pragma once

class FactionController
{
public:
    int faction_id;

    FactionController(int fid) : faction_id(fid) {};
    void update(); // runs at tick cadence
};