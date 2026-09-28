#include "state/facility.h"

Factory *Facility::createFactory()
{
    factory = std::move(std::make_unique<Factory>(this, &stores));
    return factory.get();
}

void Facility::update()
{
    // nothing yet
}

bool Barracks::hasSpace() const
{
    for (int i = 0; i < MAX_BARRACKS_CREW; ++i)
    {
        if (!crew[i])
        {
            return true;
        }
    }
    return false;
}

bool Barracks::addCrew(Crew *c)
{
    for (int i = 0; i < MAX_BARRACKS_CREW; ++i)
    {
        if (!crew[i])
        {
            crew[i] = c;
            return true;
        }
    }
    return false;
}