#pragma once

#include <vector>

class Orbital;

class FactionController
{
public:
    int faction_id;

    std::vector<const Orbital *> systemOrbitals(const int system_id);

    FactionController(int fid) : faction_id(fid) {};
    void update(); // runs at tick cadence
};