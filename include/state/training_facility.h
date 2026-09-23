#pragma once

#include "state/crew.h"

class ResourceFacility;

// training allows for one crew per type to be trained at a time
class TrainingFacility
{
public:
    Crew *scientists{nullptr};
    Crew *engineers{nullptr};
    Crew *marines{nullptr};

    TrainingFacility(ResourceFacility *facility) : facility(facility) {};

    int addTrainees(CrewType type, int count);
    int removeTrainees(CrewType type, int count);

    void update(float delta);

private:
    ResourceFacility *facility{nullptr};
    Crew *crewForType(CrewType type);
    Crew *getOrCreateCrewForType(CrewType type);
};