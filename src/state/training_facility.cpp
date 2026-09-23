#include "state/training_facility.h"
#include "state/resourceFacility.h"
#include "state/research_facility.h"
#include "state/game.h"

int TrainingFacility::addTrainees(CrewType type, int count)
{
    Crew *crew = getOrCreateCrewForType(type);
    if (crew)
    {
        // limit to crew->maxSize() - crew->size
        int max_add = crew->maxSize() - crew->size;
        if (count > max_add)
        {
            count = max_add;
        }
        crew->size += count;
    }
    return count;
}

int TrainingFacility::removeTrainees(CrewType type, int count)
{
    Crew *crew = crewForType(type);
    if (crew)
    {
        int delta = std::min(count, crew->size);
        crew->size -= delta;
        // earth_city->population += delta; // return to population pool
        if (crew->size <= 0)
        {
            // remove crew from training facility
            Game::getCurrent()->releaseCrew(crew);
            switch (type)
            {
            case CrewType::Scientist:
                scientists = nullptr;
                break;
            case CrewType::Engineer:
                engineers = nullptr;
                break;
            case CrewType::Marine:
                marines = nullptr;
                break;
            default:
                break;
            }
            crew = nullptr;
        }
        return delta;
    }
    return 0;
}

Crew *TrainingFacility::crewForType(CrewType type)
{
    switch (type)
    {
    case CrewType::Scientist:
        return scientists;
    case CrewType::Engineer:
        return engineers;
    case CrewType::Marine:
        return marines;
    default:
        return nullptr;
    }
}

Crew *TrainingFacility::getOrCreateCrewForType(CrewType type)
{
    switch (type)
    {
    case CrewType::Scientist:
        if (scientists == nullptr)
        {
            scientists = Game::getCurrent()->createCrew(0, CrewType::Scientist, "New Scientist", 0, 0, 0.0f);
        }
        return scientists;
    case CrewType::Engineer:
        if (engineers == nullptr)
        {
            engineers = Game::getCurrent()->createCrew(0, CrewType::Engineer, "New Engineer", 0, 0, 0.0f);
        }
        return engineers;
    case CrewType::Marine:
        if (marines == nullptr)
        {
            marines = Game::getCurrent()->createCrew(0, CrewType::Marine, "New Marine", 0, 0, 0.0f);
        }
        return marines;
    default:
        return nullptr;
    }
}

void TrainingFacility::update(float delta)
{
    // update training progress for each crew type
    // when they achieve enough to rank 1, training is complete
    if (scientists && scientists->addExperience(delta) > 0)
    {
        // if there is a research facility, add the trained scientists to it
        if (facility != nullptr && facility->research_facility && facility->research_facility->crew == nullptr)
        {
            facility->research_facility->assignCrew(scientists);
        }
        scientists = nullptr;
    }
    if (engineers && engineers->addExperience(delta) > 0)
    {
        // assign to local factory if not crewed
        if (facility != nullptr && facility->factory && facility->factory_crew == nullptr)
        {
            facility->assignCrew(engineers);
        }
        else
        {
            // TODO: send to barracks for hangars to reference
        }
        engineers = nullptr;
    }
    if (marines && marines->addExperience(delta) > 0)
    {
        // TODO: send to barracks for hangars to reference
        marines = nullptr;
    }
}