#include <cstdlib>
#include <cmath>

#include "loaders/loader.h"
#include "state/system.h"

System::System()
    : name{}, primary(nullptr), space(nullptr)
{
}

System::~System()
{
}

void System::update(double time)
{
    for (Location *loc : this->locations)
    {
        const float angle = static_cast<float>(std::fmod(time * loc->orbital_velocity + loc->initial_angle, 2.0 * PI));
        loc->position = (Vector2){
            loc->orbital_radius * cosf(angle),
            loc->orbital_radius * sinf(angle)};
    }
}