#include <cstdlib>
#include <cmath>

#include "state/system.h"

System::System()
    : name()
    , numPlanets(0)
{
}

Location* System::addLocation(const char* n, LocationType t) {
    this->locations.push_back(std::make_unique<Location>(this, n, t));
    // if this is a star, set it as the primary location
    if (t == LOCATION_TYPE_STAR) {
        this->primary = this->locations.back().get();
    }
    return this->locations.back().get();
}

void System::loadSolSystem()
{
    setNumBodies(8);    
    this->name = "Sol";

    this->planetDistances = std::vector<float>(this->numPlanets);
    this->planetDistances[0] = 80;
    this->planetDistances[1] = 110;
    this->planetDistances[2] = 165;
    this->planetDistances[3] = 225;
    this->planetDistances[4] = 310;
    this->planetDistances[5] = 430;
    this->planetDistances[6] = 515;
    this->planetDistances[7] = 565;

    // planet_velocities = {1.607f, 1.174f, 1.f, 0.802f, 0.434f, 0.323f, 0.228f, 0.182f};
    this->planetSizes = std::vector<float>(this->numPlanets);
    this->planetSizes[0] = 10;
    this->planetSizes[1] = 15;
    this->planetSizes[2] = 20;
    this->planetSizes[3] = 18;
    this->planetSizes[4] = 60;
    this->planetSizes[5] = 55;
    this->planetSizes[6] = 25;
    this->planetSizes[7] = 22;

    this->planetColors = std::vector<Color>(this->numPlanets);
    this->planetColors[0] = (Color){115, 147, 179, 255}; // Mercury
    this->planetColors[1] = (Color){255, 87, 51, 255}; // Venus
    this->planetColors[2] = (Color){30, 144, 255, 255}; // Earth
    this->planetColors[3] = (Color){178, 34, 34, 255}; // Mars
    this->planetColors[4] = (Color){210, 105, 30, 255}; // Jupiter
    this->planetColors[5] = (Color){220, 20, 60, 255};   // Saturn
    this->planetColors[6] = (Color){72, 209, 204, 255};  // Uranus
    this->planetColors[7] = (Color){65, 105, 225, 255};   // Neptune

    this->planetVelocities = std::vector<float>(this->numPlanets);
    this->planetVelocities[0] = 1.607f;
    this->planetVelocities[1] = 1.174f;
    this->planetVelocities[2] = 1.f;
    this->planetVelocities[3] = 0.802f;
    this->planetVelocities[4] = 0.434f;
    this->planetVelocities[5] = 0.323f;
    this->planetVelocities[6] = 0.228f;
    this->planetVelocities[7] = 0.182f;

    this->planetPositions = std::vector<Vector2>(this->numPlanets);
    for (int i = 0; i < this->numPlanets; i++)
    {        
        this->planetPositions[i] = (Vector2){this->planetDistances[i], 0};
    }

    this->planetPrimaryIndexes = std::vector<int>(this->numPlanets);
    for (int i = 0; i < this->numPlanets; i++)
    {
        this->planetPrimaryIndexes[i] = -1; // all planets orbit the star in this simple sol system
    }    
}

System& System::setNumBodies(int numBodies) {
    this->numPlanets = numBodies;
    this->planetDistances = std::vector<float>(numBodies);
    this->planetSizes = std::vector<float>(numBodies);
    this->planetColors = std::vector<Color>(numBodies);
    this->planetVelocities = std::vector<float>(numBodies);
    this->planetPositions = std::vector<Vector2>(numBodies);
    this->planetPrimaryIndexes = std::vector<int>(numBodies);
    return *this;
}

SystemPtr createSystem(const bool asSolSystem)
{
    SystemPtr system = std::make_unique<System>();

    if (asSolSystem) {        
        system->loadSolSystem();  
    } 
    return system;
}

System::~System()
{    
}

void System::update(float time)
{
    for (int i = 0; i < this->numPlanets; i++)
    {        
        float angle = time * this->planetVelocities[i];
        this->planetPositions[i] = (Vector2){
            this->planetDistances[i] * cosf(angle),
            this->planetDistances[i] * sinf(angle)
        };
    }
}