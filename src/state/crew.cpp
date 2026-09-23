#include "state/crew.h"

const char *crewTypeNames[] = {
    "Marine",
    "Engineer",
    "Scientist"};

// rank titles depend on crew type, so we have a 2D array of strings
const char *crewRankTitles[][MAX_CREW_RANK] = {
    {"Trainee", "Corporal", "Sergeant", "Lieutenant", "Captain"},               // Marine
    {"Trainee", "Technician", "Engineer", "Senior Engineer", "Chief Engineer"}, // Engineer
    {"Trainee", "Apprentice", "Doctor", "Professor", "Chief Scientist"}         // Scientist
};

char *Crew::description(char *target, size_t target_len) const
{
    std::snprintf(target, target_len, "%s %s",
                  crewRankTitles[static_cast<int>(type)][rank],
                  leader_name);
    return target;
}

int Crew::maxSize() const
{
    switch (type)
    {
    case CrewType::Marine:
        return 41; // Max size for Marines
    case CrewType::Engineer:
        return 150; // Max size for Engineers
    case CrewType::Scientist:
        return 200; // Max size for Scientists
    default:
        return 0; // Unknown type
    }
}