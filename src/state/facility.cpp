#include "state/facility.h"

Factory *Facility::createFactory()
{
    factory = std::move(std::make_unique<Factory>(&stores));
    return factory.get();
}

void Facility::update()
{
    // nothing yet
}