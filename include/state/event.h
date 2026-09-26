#pragma once
#include <vector>
#include <memory>
#include <cstring>
#include "state/strings.h"

enum EventID
{
    EVENT_NONE = 0,
    EVENT_ORBITAL_FACTORY_COMPLETED,
    EVENT_FACTION_HOSTILITY,
    EVENT_MAX
};

class Event
{
public:
    EventID id;
    char name[32];
    char log_message[256];
    char email_message[256];
    bool completed;
    double raise_at;

    std::vector<int> unlocksTopics; // research topics unlocked by this event

    Event() : id(EVENT_NONE), completed(false), raise_at(0.0)
    {
        name[0] = '\0';
        log_message[0] = '\0';
        email_message[0] = '\0';
    }

    Event(EventID i, const char *n, const char *log, const char *email, bool c, double r)
        : id(i), completed(c), raise_at(r)
    {
        copyFixed(this->name, sizeof(this->name), n);
        copyFixed(this->log_message, sizeof(this->log_message), log);
        copyFixed(this->email_message, sizeof(this->email_message), email);
    }
};

typedef std::vector<Event> Events;
