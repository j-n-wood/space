#pragma once

#include <vector>

class Orbital;
class Craft;

class FactionController
{
    int warshipRank() const;
    int garrisonThreshold() const;

public:
    int faction_id;

    std::vector<const Orbital *> systemOrbitals(const int system_id);
    std::vector<const Craft *> systemWarships(const int system_id);

    FactionController(int fid) : faction_id(fid) {};
    void update(); // runs at tick cadence
};