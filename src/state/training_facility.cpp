#include "state/training_facility.h"
#include "state/resourceFacility.h"
#include "state/research_facility.h"
#include "state/game.h"

Crew *TrainingFacility::addTrainees(CrewType type, int count)
{
    Crew *crew = crewForType(type);
    if (crew)
    {
        crew->size += count;
    }
    return crew;
}

Crew *TrainingFacility::removeTrainees(CrewType type, int count)
{
    Crew *crew = crewForType(type);
    if (crew)
    {
        crew->size -= count;
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
        }
    }
    return crew;
}

Crew *TrainingFacility::crewForType(CrewType type)
{
    switch (type)
    {
    case CrewType::Scientist:
        if (scientists == nullptr)
        {
            scientists = Game::getCurrent()->createCrew(0, CrewType::Scientist, "New Scientist", 0, 1, 0.0f);
        }
        return scientists;
    case CrewType::Engineer:
        if (engineers == nullptr)
        {
            engineers = Game::getCurrent()->createCrew(0, CrewType::Engineer, "New Engineer", 0, 1, 0.0f);
        }
        return engineers;
    case CrewType::Marine:
        if (marines == nullptr)
        {
            marines = Game::getCurrent()->createCrew(0, CrewType::Marine, "New Marine", 0, 1, 0.0f);
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