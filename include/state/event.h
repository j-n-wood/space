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
    EVENT_REQUEST_GRAPPLE,
    EVENT_GIVE_COMMSPOD_OBJECT,
    EVENT_TRADE_REQUEST,
    EVENT_WARNING,
    EVENT_NOTHING_TO_TRADE,
    EVENT_MAX
};

enum class MessageDisplayType : uint8_t
{
    NONE = 0,
    LOG,
    DIALOG
};

enum class MessageInteractionType : uint8_t
{
    NONE = 0,
    YESNO
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
    int source_faction_id;
    MessageDisplayType displayType;
    MessageInteractionType interactionType;

    std::vector<int> unlocksTopics; // research topics unlocked by this event

    Event() : id(EVENT_NONE), completed(false), raise_at(0.0), source_faction_id(0), displayType(MessageDisplayType::NONE), interactionType(MessageInteractionType::NONE)
    {
        name[0] = '\0';
        log_message[0] = '\0';
        email_message[0] = '\0';
    }

    Event(EventID i, const char *n, const char *log, const char *email, bool c, double r, int sfi, MessageDisplayType dt, MessageInteractionType it)
        : id(i), completed(c), raise_at(r), source_faction_id(sfi), displayType(dt), interactionType(it)
    {
        copyFixed(this->name, sizeof(this->name), n);
        copyFixed(this->log_message, sizeof(this->log_message), log);
        copyFixed(this->email_message, sizeof(this->email_message), email);
    }
};

typedef std::vector<Event> Events;
