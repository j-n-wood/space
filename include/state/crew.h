#pragma once

#include <cstdint>
#include <cstdio>

const int MAX_CREW_LEADER_NAME_LEN = 32;
const int MAX_CREW_RANK = 5;

enum class CrewType : uint8_t
{
    None,
    Marine,
    Engineer,
    Scientist,
    MAX_CREW_TYPE
};

class Crew
{
public:
    int id;                                     // unique identifier for the crew
    CrewType type;                              // the type of the crew
    char leader_name[MAX_CREW_LEADER_NAME_LEN]; // name of the crew leader
    int rank;
    int size;
    float experience; // experience gain to rank up

    Crew(int crew_id, CrewType crew_type, const char *leader, int crew_rank, int crew_size, float crew_experience)
        : id(crew_id), type(crew_type), rank(crew_rank), size(crew_size), experience(crew_experience)
    {
        std::snprintf(leader_name, MAX_CREW_LEADER_NAME_LEN, "%s", leader);
    }

    char *description(char *target, size_t target_len) const;

    inline int addExperience(float exp)
    {
        experience += exp;
        if (experience >= 100.0f)
        {
            experience = 0.0f;
            if (rank < MAX_CREW_RANK)
            {
                rank++;
            }
        }
        return rank;
    }
};