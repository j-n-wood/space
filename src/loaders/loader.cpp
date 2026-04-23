#include <stdlib.h>

#include "loaders/loader.h"
#include "loaders/load_system.h"
#include "raylib.h"
#include "state/game.h"
#include "state/location.h"
#include "state/resourceFacility.h"
#include "state/orbital.h"
#include "state/resources.h"

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
    SQLiteQuery query(this, "SELECT game_time FROM game LIMIT 1");

    if (query.next())
    {
        game->game_time = sqlite3_column_double(query, 0);
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
                    return loc.get();
                }
            }
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
        if (type == 0) // SLOC_SURFACE
        {
            auto rf = game->createResourceFacility(loc);
            rf->num_derricks = num_derricks;
            fac = rf;
        }
        else if (type == 1) // SLOC_ORBIT
        {
            fac = game->createOrbital(loc);
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
        SQLiteQuery query(this, "SELECT id, name, description, tool, researched, tech_level, orbital, mass, research_time, research_progress, production_time, doc_image_index, production_image_index, pod_capacity FROM items ORDER BY id");

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
            item.research_time = sqlite3_column_int(query, 8);
            item.research_progress = sqlite3_column_int(query, 9);
            item.production_time = sqlite3_column_int(query, 10);
            item.doc_image_index = sqlite3_column_int(query, 11);
            item.production_image_index = sqlite3_column_int(query, 12);
            item.pod_capacity = sqlite3_column_int(query, 13);
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