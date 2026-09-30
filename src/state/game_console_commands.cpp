#include "state/game.h"
#include "state/facility.h"
#include "state/location.h"
#include "state/orbital.h"
#include "state/shuttle.h"

bool Game::processConsoleCommand(const char *command, Location *l, Facility *f)
{
    TraceLog(LOG_INFO, "Console command entered: %s", command);

    int item_id{ItemType::None};
    int amount{0};

    if (std::strcmp(command, "help") == 0)
    {
        TraceLog(LOG_INFO, "Available commands:\n"
                           "help - show this message\n"
                           "give {item_id} {amount} - add items to current facility\n"
                           "orbital - create an orbital at the current location for testing\n"
                           "shuttle - spawn shuttle at current facility for testing\n"
                           "research {topic_id} - set research progress for a topic (use -1 for all topics)");
        return true;
    }
    else if (std::sscanf(command, "give %d %d", &item_id, &amount) == 2) // give {item id} {amount} command to add items to current location for testing
    {
        if (f)
        {
            f->stores.items[item_id] += amount;
            TraceLog(LOG_INFO, "Added %d of item %d to current location %s", amount, item_id, f->body()->name);
            return true;
        }
    }
    // 'orbital' command to create an orbital at the current location for testing, if not present
    else if (std::strcmp(command, "orbital") == 0)
    {
        if (!orbitalAt(l))
        {
            Orbital *orbital{createOrbital(l)};
            orbital->operational = true;
            TraceLog(LOG_INFO, "Orbital created at location: %s", l->name);
            return true;
        }
    }
    // 'shuttle' to spawn shuttle at current facility if any
    else if (std::strcmp(command, "shuttle") == 0)
    {
        if (f && !locationHasShuttle(f->body())) // shuttles hang off the body, not the region
        {
            Shuttle *shuttle{createShuttle(f->body())};
            if (shuttle)
            {
                setDefaultRoute(shuttle, f); // debug spawn, so no chassis cost
            }
            TraceLog(LOG_INFO, "Shuttle created at facility: %s", f->body()->name);
            return true;
        }
    }
    // research {topic id} command to set research progress for testing
    else if (std::sscanf(command, "research %d", &item_id) == 1)
    {
        if (item_id == -1)
        {
            // set all research to complete
            for (auto &topic : researchTopics)
            {
                topic.progress = topic.requiredTime;
            }
            // set all items to researched as well
            for (auto &item : items)
            {
                item.researched = true;
            }
            TraceLog(LOG_INFO, "All research topics set to complete");
        }
        else if (item_id >= 0 && item_id < researchTopics.size())
        {
            researchTopics[item_id].progress = researchTopics[item_id].requiredTime;
            TraceLog(LOG_INFO, "Research topic %d set to complete", item_id);
        }
        return true;
    }
    else if (std::strcmp(command, "sdm") == 0)
    {
        if (f)
        {
            if (f->sdm_active)
            {
                deactivateSDM(f);
                TraceLog(LOG_INFO, "SDM deactivated at facility: %s", f->body()->name);
            }
            else
            {
                activateSDM(f);
                TraceLog(LOG_INFO, "SDM activated at facility: %s", f->body()->name);
            }
            return true;
        }
    }

    return false;
}