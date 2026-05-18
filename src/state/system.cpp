#include <cstdlib>
#include <cmath>

#include "loaders/loader.h"
#include "state/system.h"

System::System()
    : name{}, numPlanets(0)
{
}

System &System::setNumBodies(int numBodies)
{
    this->numPlanets = numBodies;
    this->planetDistances = std::vector<float>(numBodies);
    this->planetSizes = std::vector<float>(numBodies);
    this->planetColors = std::vector<Color>(numBodies);
    this->planetVelocities = std::vector<float>(numBodies);
    this->planetInitialAngles = std::vector<float>(numBodies);
    this->planetPositions = std::vector<Vector2>(numBodies);
    this->planetPrimaryIndexes = std::vector<int>(numBodies);
    return *this;
}

System::~System()
{
}

void System::update(float time)
{
    for (int i = 0; i < this->numPlanets; i++)
    {
        float angle = time * this->planetVelocities[i] + this->planetInitialAngles[i];
        this->planetPositions[i] = (Vector2){
            this->planetDistances[i] * cosf(angle),
            this->planetDistances[i] * sinf(angle)};
    }
}

// obtain cartestian position of a body by recursively resolving parent positions based on primary indexes
Vector2 System::getResolvedPosition(const int index)
{
    if (index < 0 || index >= this->numPlanets)
    {
        return {0.0f, 0.0f};
    }
    int parent_index = this->planetPrimaryIndexes[index];
    Vector2 parent_pos = {0.0f, 0.0f};
    if (parent_index >= 0)
    {
        parent_pos = getResolvedPosition(parent_index);
    }
    return {
        parent_pos.x + this->planetPositions[index].x,
        parent_pos.y + this->planetPositions[index].y};
}