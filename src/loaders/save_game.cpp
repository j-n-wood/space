#include "loaders/save_game.h"
#include "loaders/loader.h"
#include "state/game.h"
#include "state/location.h"
#include "state/system.h"
#include "state/resourceFacility.h"
#include "state/orbital.h"
#include "state/stores.h"
#include "state/resources.h"

#include <cstdio>
#include <cstring>
#include <cmath>

extern "C"
{
#include "raylib.h"
}

namespace
{
    struct ScopedSqliteError
    {
        char *ptr = nullptr;

        operator char **()
        {
            return &ptr;
        }

        const char *get() const
        {
            return ptr;
        }

        ~ScopedSqliteError()
        {
            if (ptr)
            {
                sqlite3_free(ptr);
            }
        }
    };

    void ColorToHexString(const Color &color, char (&out)[9])
    {
        std::snprintf(out, sizeof out, "%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
    }
}

// Save game implementation

// This will write to a new SQLite file with the same structure as read by e.g. load_system

/*
Schema:

CREATE TABLE bodies ( id INTEGER PRIMARY key, system_id INT, primary_id INT, type INT, name text, orbital_radius float, orbital_velocity float, initial_angle float, radius float, color text )
CREATE TABLE systems ( id INTEGER primary key, name text )
CREATE TABLE facilities ( id int, system_id int, location_id int, type int )
CREATE TABLE stores ( facility_id int, resource_id int, amount int )
CREATE TABLE game ( game_time FLOAT )
CREATE TABLE items ( id int, name text, description text, tool int, researched int, tech_level int, orbital int, mass int, research_time float, research_remaining float, production_time float, doc_image_index int, production_image_index int, pod_capacity int);
CREATE TABLE item_build_requirements ( item_id int, resource_id int, amount int);
*/

SaveGame::SaveGame()
    : loader(nullptr)
{
    // Initialize SaveGame instance
}

SaveGame::~SaveGame()
{
    delete loader;
    loader = nullptr;
}

int SaveGame::save(const char *filename)
{
    // Validate input
    if (!filename || std::strlen(filename) == 0)
    {
        TraceLog(LOG_ERROR, "SaveGame: Invalid filename");
        return -1;
    }

    // clobber existing file
    if (FileExists(filename))
    {
        TraceLog(LOG_WARNING, "SaveGame: Removing existing file");
        if (remove(filename) != 0)
        {
            // failed to remove
            TraceLog(LOG_ERROR, "SaveGame: Failed to remove existing file");
        }
    }

    delete loader;
    loader = new Loader(filename);
    if (!loader->isValid())
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to open database: %s", filename);
        delete loader;
        loader = nullptr;
        return -2;
    }

    int initResult = initialiseSaveFile();
    if (initResult != 0)
    {
        return initResult;
    }

    // Save game state
    int result = saveGame(Game::getCurrent());
    if (result != 0)
    {
        return result;
    }

    return 0; // Success
}

int SaveGame::initialiseSaveFile()
{
    if (!loader || !loader->db)
    {
        TraceLog(LOG_ERROR, "SaveGame: Invalid loader provided to initialiseSaveFile");
        return -3;
    }

    const char *schema =
        "BEGIN TRANSACTION;"
        "CREATE TABLE IF NOT EXISTS bodies ( id INTEGER, system_id INT, primary_id INT, type INT, name TEXT, orbital_radius FLOAT, orbital_velocity FLOAT, initial_angle FLOAT, radius FLOAT, color TEXT );"
        "CREATE TABLE IF NOT EXISTS systems ( id INTEGER, name TEXT );"
        "CREATE TABLE IF NOT EXISTS facilities ( id INT, system_id INT, location_id INT, type INT, num_derricks INT);"
        "CREATE TABLE IF NOT EXISTS stores ( facility_id INT, resource_id INT, amount INT );"
        "CREATE TABLE IF NOT EXISTS game ( game_time FLOAT, ios_number INT, scg_number INT );"
        "CREATE TABLE IF NOT EXISTS items ( id int, name text, description text, tool int, researched int, tech_level int, orbital int, mass int, research_time float, research_remaining float, production_time float, doc_image_index int, production_image_index int, pod_capacity int);"
        "CREATE TABLE IF NOT EXISTS item_build_requirements ( item_id int, resource_id int, amount int);"
        "COMMIT;";

    ScopedSqliteError errorMessage;
    int rc = sqlite3_exec(loader->db, schema, nullptr, nullptr, errorMessage);
    if (rc != SQLITE_OK)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to initialise save file schema: %s", errorMessage.get() ? errorMessage.get() : "Unknown error");
        return -4;
    }

    return 0;
}

int SaveGame::saveGame(Game *game)
{
    // Validate input
    if (!loader || !loader->db)
    {
        TraceLog(LOG_ERROR, "SaveGame: Null loader pointer");
        return -3;
    }

    if (!game)
    {
        TraceLog(LOG_ERROR, "SaveGame: Null game pointer");
        return -4;
    }

    ScopedSqliteError errorMessage;
    char sql[256];
    const char *clearSql = "DELETE FROM game; DELETE FROM stores; DELETE FROM facilities; DELETE FROM bodies; DELETE FROM systems;";
    int rc = sqlite3_exec(loader->db, clearSql, nullptr, nullptr, errorMessage);
    if (rc != SQLITE_OK)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to clear save tables: %s", errorMessage.get() ? errorMessage.get() : "Unknown error");
        return -5;
    }

    std::snprintf(sql, sizeof(sql), "INSERT INTO game (game_time, ios_number, scg_number) VALUES (%f, %d, %d);", game->game_time, game->ios_number, game->scg_number);
    rc = sqlite3_exec(loader->db, sql, nullptr, nullptr, errorMessage);
    if (rc != SQLITE_OK)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to insert game_time: %s", errorMessage.get() ? errorMessage.get() : "Unknown error");
        return -6;
    }

    int nextFacilityId = 1;
    for (auto &system : game->allSystems())
    {
        if (system && saveSystem(system.get()) != 0)
        {
            return -7; // error logged at failure site
        }
    }

    for (auto &base : game->allBases())
    {
        if (base && saveBase(base.get(), nextFacilityId++) != 0)
        {
            return -8;
        }
    }

    for (auto &orbital : game->allOrbitals())
    {
        if (orbital && saveOrbital(orbital.get(), nextFacilityId++) != 0)
        {
            return -9;
        }
    }

    if (saveItems(game) != 0)
    {
        return -10;
    }

    return 0;
}

int SaveGame::saveSystem(System *system)
{
    if (!loader || !loader->db)
    {
        TraceLog(LOG_ERROR, "SaveGame: Null loader pointer");
        return -4;
    }

    if (!system)
    {
        TraceLog(LOG_ERROR, "SaveGame: Null system pointer");
        return -5;
    }

    SQLiteQuery systemQuery(loader, "INSERT INTO systems (id, name) VALUES (?, ?);");
    if (!systemQuery.stmt)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to prepare systems insert");
        return -6;
    }

    if (!systemQuery.bind(1, system->id)
             .bind(2, system->name)
             .step("SaveGame: Failed to execute systems insert"))
    {
        return -8;
    }

    sqlite3_int64 systemId = system->id;

    const char *bodySql =
        "INSERT INTO bodies (id, system_id, primary_id, type, name, orbital_radius, orbital_velocity, initial_angle, radius, color) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    SQLiteQuery bodyQuery(loader, bodySql);
    if (!bodyQuery.stmt)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to prepare bodies insert");
        return -10;
    }

    size_t bodyCount = system->locations.size();
    for (size_t idx = 0; idx < bodyCount; ++idx)
    {
        int result = saveLocation(bodyQuery, system, idx, systemId);
        if (result != 0)
        {
            return result;
        }
    }

    return 0;
}

int SaveGame::saveLocation(SQLiteQuery &bodyQuery, System *system, size_t locationIndex, sqlite3_int64 systemId)
{
    if (!bodyQuery.stmt)
    {
        TraceLog(LOG_ERROR, "SaveGame: Null SQL statement in saveLocation");
        return -11;
    }

    if (!system)
    {
        TraceLog(LOG_ERROR, "SaveGame: Null system pointer in saveLocation");
        return -12;
    }

    if (locationIndex >= system->locations.size())
    {
        TraceLog(LOG_ERROR, "SaveGame: Location index out of range");
        return -13;
    }

    Location *location = system->locations[locationIndex].get();
    if (!location)
    {
        return 0;
    }

    if (location->id == 0)
    {
        TraceLog(LOG_ERROR, "SaveGame: Cannot save location with missing ID");
        return -14;
    }

    const float orbitalRadius = locationIndex < system->planetDistances.size() ? system->planetDistances[locationIndex] : 0.0f;
    const float orbitalVelocity = locationIndex < system->planetVelocities.size() ? system->planetVelocities[locationIndex] : 0.0f;
    const float initialAngle = locationIndex < system->planetPositions.size() ? atan2f(system->planetPositions[locationIndex].y, system->planetPositions[locationIndex].x) : 0.0f;
    const float radius = locationIndex < system->planetSizes.size() ? system->planetSizes[locationIndex] : 0.0f;
    char colorText[9];
    if (locationIndex < system->planetColors.size())
    {
        ColorToHexString(system->planetColors[locationIndex], colorText);
    }
    else
    {
        std::snprintf(colorText, sizeof colorText, "FFFFFFFF");
    }

    sqlite3_reset(bodyQuery.stmt);
    sqlite3_clear_bindings(bodyQuery.stmt);

    if (!bodyQuery.bind(1, location->id)
             .bind(2, systemId)
             .bind(3, location->primary_id)
             .bind(4, static_cast<int>(location->type))
             .bind(5, location->name)
             .bind(6, orbitalRadius)
             .bind(7, orbitalVelocity)
             .bind(8, initialAngle)
             .bind(9, radius)
             .bind(10, colorText)
             .step("SaveGame: Failed to insert body record"))
    {
        return -15;
    }

    return 0;
}

int SaveGame::saveBase(ResourceFacility *rf, int facilityId)
{
    if (!loader || !loader->db)
    {
        TraceLog(LOG_ERROR, "SaveGame: Null loader pointer");
        return -6;
    }

    if (!rf || !rf->location)
    {
        TraceLog(LOG_ERROR, "SaveGame: Null resource facility or location pointer");
        return -7;
    }

    if (!rf->location->system)
    {
        TraceLog(LOG_ERROR, "SaveGame: Resource facility location has no system");
        return -8;
    }

    SQLiteQuery facilityQuery(loader, "INSERT INTO facilities (id, system_id, location_id, type, num_derricks) VALUES (?, ?, ?, ?, ?);");
    if (!facilityQuery.stmt)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to prepare facility insert for base");
        return -9;
    }

    if (!facilityQuery.bind(1, facilityId)
             .bind(2, rf->location->system->id)
             .bind(3, rf->location->id)
             .bind(4, SLOC_SURFACE)
             .bind(5, (int)rf->num_derricks)
             .step("SaveGame: Failed to execute facility insert for base"))
    {
        return -14;
    }

    return saveStores(&rf->stores, facilityId);
}

int SaveGame::saveOrbital(Orbital *orbital, int facilityId)
{
    if (!loader || !loader->db)
    {
        TraceLog(LOG_ERROR, "SaveGame: Null loader pointer");
        return -6;
    }

    if (!orbital || !orbital->location)
    {
        TraceLog(LOG_ERROR, "SaveGame: Null orbital or location pointer");
        return -7;
    }

    if (!orbital->location->system)
    {
        TraceLog(LOG_ERROR, "SaveGame: Orbital location has no system");
        return -8;
    }

    SQLiteQuery facilityQuery(loader, "INSERT INTO facilities (id, system_id, location_id, type) VALUES (?, ?, ?, ?);");
    if (!facilityQuery.stmt)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to prepare facility insert for orbital");
        return -9;
    }

    if (!facilityQuery.bind(1, facilityId)
             .bind(2, orbital->location->system->id)
             .bind(3, orbital->location->id)
             .bind(4, SLOC_ORBIT)
             .step("SaveGame: Failed to execute facility insert for orbital"))
    {
        return -14;
    }

    return saveStores(&orbital->stores, facilityId);
}

int SaveGame::saveStores(Stores *stores, int facilityId)
{
    if (!loader || !loader->db)
    {
        TraceLog(LOG_ERROR, "SaveGame: Null loader pointer");
        return -6;
    }

    if (!stores)
    {
        TraceLog(LOG_ERROR, "SaveGame: Null stores pointer");
        return -8;
    }

    SQLiteQuery storesQuery(loader, "INSERT INTO stores (facility_id, resource_id, amount) VALUES (?, ?, ?);");
    if (!storesQuery.stmt)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to prepare stores insert");
        return -9;
    }

    for (int resourceId = 1; resourceId < ResourceType::Count; ++resourceId)
    {
        int amount = stores->resources[resourceId];
        if (amount == 0)
        {
            continue;
        }

        sqlite3_reset(storesQuery.stmt);
        sqlite3_clear_bindings(storesQuery.stmt);

        if (!storesQuery.bind(1, facilityId)
                 .bind(2, resourceId)
                 .bind(3, amount)
                 .step("SaveGame: Failed to execute stores insert"))
        {
            return -13;
        }
    }

    return 0;
}

int SaveGame::saveItems(Game *game)
{
    int idx{0};

    struct ItemBuildRequirement
    {
        int item_id;
        int resource_id;
        int amount;
    };

    std::vector<ItemBuildRequirement> reqs;
    {
        SQLiteQuery itemQ(loader, "INSERT INTO items (id, name, description, tool, researched, tech_level, orbital, mass, research_time, research_remaining, production_time, doc_image_index, production_image_index, pod_capacity) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
        for (auto &item : game->items)
        {
            sqlite3_reset(itemQ.stmt);
            sqlite3_clear_bindings(itemQ.stmt);
            if (!itemQ.bind(1, idx).bind(2, item.name).bind(3, item.description).bind(4, item.tool).bind(5, item.researched).bind(6, item.tech_level).bind(7, item.orbital).bind(8, item.mass).bind(9, item.research_time).bind(10, item.research_remaining).bind(11, item.production_time).bind(12, item.doc_image_index).bind(13, item.production_image_index).bind(14, item.pod_capacity).step("SaveGame: Failed to save item"))
            {
                return -15;
            }

            for (auto &br : item.requirements)
            {
                reqs.emplace_back(ItemBuildRequirement{idx, br.resource, br.amount});
            }

            ++idx;
        }
    }

    {
        SQLiteQuery reqQ(loader, "INSERT INTO item_build_requirements (item_id, resource_id, amount) VALUES (?, ?, ?)");
        for (auto &br : reqs)
        {
            sqlite3_reset(reqQ.stmt);
            sqlite3_clear_bindings(reqQ.stmt);
            if (!reqQ.bind(1, br.item_id).bind(2, br.resource_id).bind(3, br.amount).step("SaveGame: Failed to save build req"))
            {
                return -16;
            }
        }
    }
    return 0;
}