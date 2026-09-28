#pragma once

#include <cstdint>
#include <cstdio>

const int MAX_CREW_LEADER_NAME_LEN = 32;
const int MAX_CREW_RANK = 5;
const float experience_thresholds[MAX_CREW_RANK] = {20.0f, 70.0f, 120.0f, 200.0f, 300.0f};

enum class CrewType : uint8_t
{
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
        while (rank < MAX_CREW_RANK && experience >= experience_thresholds[rank])
        {
            experience -= experience_thresholds[rank];
            rank++;
        }
        return rank;
    }

    int maxSize() const;
    bool inTraining() const
    {
        return (rank == 0 && experience > 0.0f);
    }
};