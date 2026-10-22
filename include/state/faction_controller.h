#pragma once

#include <vector>

class Orbital;
class Craft;
class Location;

class FactionController
{
    int warshipRank() const;
    int garrisonThreshold() const;

public:
    int faction_id;

    std::vector<Orbital *> systemOrbitals(const int system_id);
    std::vector<Craft *> systemWarships(const int system_id);

    Location *selectTargetLocation(Craft *warship);

    FactionController(int fid) : faction_id(fid) {};
    void update(); // runs at tick cadence
};