#pragma once

class Crew;

class ResearchFacility
{
public:
    int current_project{-1}; // index into project definitions, -1 for none
    Crew *crew;

    ResearchFacility() : crew{nullptr} {};

    inline Crew *assignCrew(Crew *c)
    {
        auto prior = crew;
        crew = c;
        return prior;
    }

    void update(float delta);
};