#pragma once

class ResearchFacility
{
    int current_project{-1}; // index into project definitions, -1 for none
    float project_time{0};   // time remaining on current project
public:
    ResearchFacility() {};

    void update(float delta);
};