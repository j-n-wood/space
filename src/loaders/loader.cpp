#include <cstdlib>
#include <cstdio>

#include "loaders/loader.h"
#include "loaders/load_system.h"
#include "raylib.h"
#include "state/game.h"
#include "state/location.h"
#include "state/resourceFacility.h"
#include "state/orbital.h"
#include "state/resources.h"
#include "state/earth_city.h"
#include "state/autopilot.h"

Loader::Loader(const char *dbPath) : game(nullptr)
{
    if (sqlite3_open(dbPath, &db) != SQLITE_OK)
    {
        // sqlite3_open cleans up on failure, don't close
        db = nullptr;
        TraceLog(LOG_ERROR, "Failed to open database: %s", sqlite3_errmsg(db));
    }
}

Loader::~Loader()
{
    if (db)
    {
        sqlite3_close(db);
        db = nullptr;
    }
}

bool Loader::loadGame()
{
    SQLiteQuery query(this, "SELECT game_time, ios_number, scg_number FROM game LIMIT 1");

    if (query.next())
    {
        game->game_time = sqlite3_column_double(query, 0);
        game->ios_number = sqlite3_column_int(query, 1);
        game->scg_number = sqlite3_column_int(query, 2);
        return true;
    }

    TraceLog(LOG_ERROR, "Failed to load game state");
    return false;
}

Location *Loader::findLocation(int system_id, int location_id)
{
    for (auto &sys : game->allSystems())
    {
        if (sys->id == system_id)
        {
            for (auto &loc : sys->locations)
            {
                if (loc->id == location_id)
                {
                    return loc;
                }
            }
            return nullptr; // system found but location not found
        }
    }
    return nullptr;
}

bool Loader::loadFacilities()
{
    SQLiteQuery query(this, "SELECT id, system_id, location_id, type, num_derricks FROM facilities ORDER BY id");

    while (query.next())
    {
        int id = sqlite3_column_int(query, 0);
        int system_id = sqlite3_column_int(query, 1);
        int location_id = sqlite3_column_int(query, 2);
        int type = sqlite3_column_int(query, 3);
        int num_derricks = sqlite3_column_int(query, 4);

        Location *loc = findLocation(system_id, location_id);
        if (!loc)
        {
            TraceLog(LOG_ERROR, "Failed to find location %d in system %d for facility %d", location_id, system_id, id);
            return false;
        }

        Facility *fac = nullptr;
        if (type == SLOC_SURFACE)
        {
            auto rf = game->createResourceFacility(loc);
            rf->num_derricks = num_derricks;
            fac = rf;
        }
        else if (type == SLOC_ORBIT)
        {
            fac = game->createOrbital(loc);
        }
        else if (type == SLOC_EARTH_CITY)
        {
            auto ec = game->createEarthCity(loc);
            ec->num_derricks = num_derricks;
            fac = ec;
        }
        else
        {
            TraceLog(LOG_ERROR, "Unknown facility type %d for facility %d", type, id);
            return false;
        }

        if (fac)
        {
            fac->id = id;
        }
    }

    return true;
}

bool Loader::loadStores()
{
    SQLiteQuery query(this, "SELECT facility_id, resource_id, amount FROM stores");

    while (query.next())
    {
        int facility_id = sqlite3_column_int(query, 0);
        int resource_id = sqlite3_column_int(query, 1);
        int amount = sqlite3_column_int(query, 2);

        Facility *fac = nullptr;
        // Find facility by id
        for (auto &base : game->allBases())
        {
            if (base->id == facility_id)
            {
                fac = base.get();
                break;
            }
        }
        if (!fac)
        {
            for (auto &orb : game->allOrbitals())
            {
                if (orb->id == facility_id)
                {
                    fac = orb.get();
                    break;
                }
            }
        }

        if (!fac)
        {
            TraceLog(LOG_ERROR, "Failed to find facility with id %d for stores", facility_id);
            return false;
        }

        if (resource_id >= 0 && resource_id < ResourceType::Count)
        {
            fac->stores.resources[resource_id] = amount;
        }
        else
        {
            TraceLog(LOG_ERROR, "Invalid resource_id %d for facility %d", resource_id, facility_id);
        }
    }

    return true;
}

bool Loader::loadItems()
{
    game->items.clear();

    {
        SQLiteQuery query(this, "SELECT id, name, description, tool, researched, tech_level, orbital, mass, production_time, doc_image_index, production_image_index, pod_capacity FROM items ORDER BY id");

        while (query.next())
        {
            Item item;
            item.id = sqlite3_column_int(query, 0);
            copyFixed(item.name, sizeof item.name, (const char *)sqlite3_column_text(query, 1));
            copyFixed(item.description, sizeof item.description, (const char *)sqlite3_column_text(query, 2));
            item.tool = sqlite3_column_int(query, 3) > 0;
            item.researched = sqlite3_column_int(query, 4) > 0;
            item.tech_level = sqlite3_column_int(query, 5);
            item.orbital = sqlite3_column_int(query, 6) > 0;
            item.mass = sqlite3_column_int(query, 7);
            item.production_time = (float)sqlite3_column_double(query, 8);
            item.doc_image_index = sqlite3_column_int(query, 9);
            item.production_image_index = sqlite3_column_int(query, 10);
            item.pod_capacity = sqlite3_column_int(query, 11);
            game->items.push_back(item);
        }
    }

    auto n_items{game->items.size()};

    {
        SQLiteQuery query(this, "SELECT item_id, resource_id, amount FROM item_build_requirements");
        while (query.next())
        {
            int item_id = sqlite3_column_int(query, 0);
            int resource_id = sqlite3_column_int(query, 1);
            int amount = sqlite3_column_int(query, 2);

            if (item_id < n_items)
            {
                game->items[item_id].requirements.emplace_back(BuildRequirement{ResourceType(resource_id), amount});
            }
            else
            {
                TraceLog(LOG_ERROR, "Invalid item ID", item_id);
            }
        }
    }

    return true;
}

bool Loader::loadResearchTopics()
{
    game->researchTopics.clear();

    {
        SQLiteQuery query(this, "SELECT id, name, description, required_time, progress, available FROM research_topics ORDER BY id");

        while (query.next())
        {
            ResearchTopic topic;
            topic.id = sqlite3_column_int(query, 0);
            copyFixed(topic.name, sizeof topic.name, (const char *)sqlite3_column_text(query, 1));
            copyFixed(topic.description, sizeof topic.description, (const char *)sqlite3_column_text(query, 2));
            topic.requiredTime = (float)sqlite3_column_double(query, 3);
            topic.progress = (float)sqlite3_column_double(query, 4);
            topic.available = sqlite3_column_int(query, 5) > 0;
            game->researchTopics.push_back(topic);
        }
    }

    auto n_topics{game->researchTopics.size()};

    {
        SQLiteQuery query(this, "SELECT topic_id, item_id FROM research_topic_unlocks_items");
        while (query.next())
        {
            int topic_id = sqlite3_column_int(query, 0);
            int item_id = sqlite3_column_int(query, 1);

            if (topic_id < n_topics)
            {
                game->researchTopics[topic_id].unlocksItems.push_back(ItemType(item_id));
            }
            else
            {
                TraceLog(LOG_ERROR, "Invalid research topic ID", topic_id);
            }
        }
    }

    {
        SQLiteQuery query(this, "SELECT topic_id, unlocks_topic_id FROM research_topic_unlocks_topics");
        while (query.next())
        {
            int topic_id = sqlite3_column_int(query, 0);
            int unlocks_topic_id = sqlite3_column_int(query, 1);

            if (topic_id < n_topics)
            {
                game->researchTopics[topic_id].unlocksTopics.push_back(unlocks_topic_id);
            }
            else
            {
                TraceLog(LOG_ERROR, "Invalid research topic ID", topic_id);
            }
        }
    }
    return true;
}

bool Loader::loadCraft()
{
    // read craft and create instances

    SQLiteQuery destQuery(this, "SELECT system_id, location_id, sublocation, docked FROM craft_destinations WHERE craft_id = ? ORDER BY destination_index");
    if (!destQuery.stmt)
    {
        TraceLog(LOG_ERROR, "Failed to prepare craft_destinations query");
        return false;
    }

    SQLiteQuery autopilotQuery(this, "SELECT state FROM craft_autopilot WHERE craft_id = ?");
    if (!autopilotQuery.stmt)
    {
        TraceLog(LOG_ERROR, "Failed to prepare autopilot query for craft");
        return false;
    }

    // autopilot flow settings
    SQLiteQuery autopilotFlowsQuery(this, "SELECT resource_index, flow_flags FROM craft_autopilot_flows WHERE craft_id = ?");
    if (!autopilotFlowsQuery.stmt)
    {
        TraceLog(LOG_ERROR, "Failed to prepare autopilot_flows query for craft");
        return false;
    }

    // AP cursors - one per endpoint
    SQLiteQuery autopilotCursorsQuery(this, "SELECT endpoint_index, cursor_position FROM craft_autopilot_cursors WHERE craft_id = ?");
    if (!autopilotCursorsQuery.stmt)
    {
        TraceLog(LOG_ERROR, "Failed to prepare autopilot_cursors query for craft");
        return false;
    }

    SQLiteQuery query(this, "SELECT id, name, type, state, state_timer, total_state_timer, location_id, fuel, max_pods, drive, destination_index FROM craft");
    while (query.next())
    {
        int id = sqlite3_column_int(query, 0);
        const char *name = (const char *)sqlite3_column_text(query, 1);
        int type = sqlite3_column_int(query, 2);
        int state = sqlite3_column_int(query, 3);
        float state_timer = (float)sqlite3_column_double(query, 4);
        float total_state_timer = (float)sqlite3_column_double(query, 5);
        int location_id = sqlite3_column_int(query, 6);
        int fuel = sqlite3_column_int(query, 7);
        int max_pods = sqlite3_column_int(query, 8);
        bool drive = sqlite3_column_int(query, 9) > 0;
        int destination_index = sqlite3_column_int(query, 10);

        // TODO do we need system ID?
        Location *loc = game->locationByID(location_id);
        if ((location_id > 0) && (!loc))
        {
            TraceLog(LOG_ERROR, "Failed to find location %d for craft %d", location_id, id);
            return false;
        } // otherwise in the 'space' location (0)

        Craft *craft = nullptr;
        if (type == CT_SHUTTLE)
        {
            craft = game->createShuttle(loc);
        }
        else if (type == CT_IOS)
        {
            craft = game->createIOS(loc);
        }
        else
        {
            TraceLog(LOG_ERROR, "Unknown craft type %d for craft %d", type, id);
            return false;
        }

        if (!craft)
        {
            TraceLog(LOG_ERROR, "Failed to create craft instance for craft %d", id);
            return false;
        }

        std::snprintf(craft->name, sizeof craft->name, "%s", name);
        craft->state = CraftState(state);
        craft->state_timer = state_timer;
        craft->total_state_timer = total_state_timer;
        craft->fuel = fuel;
        craft->max_pods = max_pods;
        craft->drive = drive;
        craft->destination_index = destination_index;

        // load destinations for this craft

        if (!destQuery.reset().bind(1, id))
        {
            TraceLog(LOG_ERROR, "Failed to bind craft_id for craft_destinations query for craft %d", id);
            return false;
        }
        int destIndex = 0;
        while (destQuery.next())
        {
            int system_id = sqlite3_column_int(destQuery, 0);
            int dest_location_id = sqlite3_column_int(destQuery, 1);
            int sublocation = sqlite3_column_int(destQuery, 2);
            bool docked = sqlite3_column_int(destQuery, 3) > 0;
            craft->destinations[destIndex] = Endpoint(findLocation(system_id, dest_location_id), SublocationType(sublocation), docked);
            destIndex++;
        }

        // load autopilot for this craft if any

        if (!autopilotQuery.reset().bind(1, id))
        {
            TraceLog(LOG_ERROR, "Failed to bind craft_id for autopilot query for craft %d", id);
            return false;
        }

        if (autopilotQuery.next())
        {
            int ap_state = sqlite3_column_int(autopilotQuery, 0) > 0;
            craft->autopilot->state = (AutopilotState)ap_state;
        }

        // load autopilot flows for this craft if any

        if (!autopilotFlowsQuery.reset().bind(1, id))
        {
            TraceLog(LOG_ERROR, "Failed to bind craft_id for autopilot_flows query for craft %d", id);
            return false;
        }

        while (autopilotFlowsQuery.next())
        {
            int resource_index = sqlite3_column_int(autopilotFlowsQuery, 0);
            int flow_flags = sqlite3_column_int(autopilotFlowsQuery, 1);
            craft->autopilot->flow[resource_index] = flow_flags;
        }

        // load autopilot cursors for this craft if any

        if (!autopilotCursorsQuery.reset().bind(1, id))
        {
            TraceLog(LOG_ERROR, "Failed to bind craft_id for autopilot_cursors query for craft %d", id);
            return false;
        }

        while (autopilotCursorsQuery.next())
        {
            int endpoint_index = sqlite3_column_int(autopilotCursorsQuery, 0);
            float cursor_position = (float)sqlite3_column_double(autopilotCursorsQuery, 1);
            craft->autopilot->cursors[endpoint_index] = cursor_position;
        }
    }
    return true;
}