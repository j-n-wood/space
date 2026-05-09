#pragma once

class ResearchFacility
{
public:
    int current_project{-1}; // index into project definitions, -1 for none

    ResearchFacility() {};

    void update(float delta);
};