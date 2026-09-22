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

    Crew *addTrainees(CrewType type, int count);
    Crew *removeTrainees(CrewType type, int count);

    void update(float delta);

private:
    ResourceFacility *facility{nullptr};
    Crew *crewForType(CrewType type);
};