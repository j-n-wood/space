#include "state/crew.h"

const char *crewTypeNames[] = {
    "None",
    "Marine",
    "Engineer",
    "Scientist"};

// rank titles depend on crew type, so we have a 2D array of strings
const char *crewRankTitles[][MAX_CREW_RANK] = {
    {"None", "None", "None", "None", "None"},                                   // None
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