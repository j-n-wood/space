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
#include <string>

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

    std::string ColorToHexString(const Color &color)
    {
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
        return std::string(buffer);
    }
}

// Save game implementation

// This will write to a new SQLite file with the same structure as read by e.g. load_system

/*
Schema:

CREATE TABLE bodies ( id INTEGER PRIMARY key, system_id INT, primary_id INT, type INT, name text, orbital_radius float, orbital_velocity float, initial_angle float, radius float, color text )
CREATE TABLE systems ( id INTEGER primary key, name text )
CREATE TABLE facility ( id int, system_id int, location_id int, type int )
CREATE TABLE stores ( facility_id int, resource_id int, amount int )
CREATE TABLE game ( game_time int )
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

    // Get the game instance
    Game &game = Game::getInstance();

    // Save game state
    int result = saveGame(&game);
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
        "CREATE TABLE IF NOT EXISTS facility ( id INT, system_id INT, location_id INT, type INT );"
        "CREATE TABLE IF NOT EXISTS stores ( facility_id INT, resource_id INT, amount INT );"
        "CREATE TABLE IF NOT EXISTS game ( game_time INT );"
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
    const char *clearSql = "DELETE FROM game; DELETE FROM stores; DELETE FROM facility; DELETE FROM bodies; DELETE FROM systems;";
    int rc = sqlite3_exec(loader->db, clearSql, nullptr, nullptr, errorMessage);
    if (rc != SQLITE_OK)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to clear save tables: %s", errorMessage.get() ? errorMessage.get() : "Unknown error");
        return -5;
    }

    std::snprintf(sql, sizeof(sql), "INSERT INTO game (game_time) VALUES (%u);", game->game_time);
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

    return 0; // TODO: Implement full game save logic
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

    int rc = sqlite3_bind_int(systemQuery.stmt, 1, system->id);
    if (rc != SQLITE_OK)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to bind system id");
        return -7;
    }

    rc = sqlite3_bind_text(systemQuery.stmt, 2, system->name.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to bind system name");
        return -7;
    }

    if (!systemQuery.step("SaveGame: Failed to execute systems insert"))
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
    const std::string colorText = locationIndex < system->planetColors.size() ? ColorToHexString(system->planetColors[locationIndex]) : std::string("FFFFFFFF");

    sqlite3_reset(bodyQuery.stmt);
    sqlite3_clear_bindings(bodyQuery.stmt);

    sqlite3_bind_int(bodyQuery.stmt, 1, location->id);
    sqlite3_bind_int64(bodyQuery.stmt, 2, systemId);
    sqlite3_bind_int(bodyQuery.stmt, 3, location->primary_id);
    sqlite3_bind_int(bodyQuery.stmt, 4, static_cast<int>(location->type));
    sqlite3_bind_text(bodyQuery.stmt, 5, location->name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(bodyQuery.stmt, 6, orbitalRadius);
    sqlite3_bind_double(bodyQuery.stmt, 7, orbitalVelocity);
    sqlite3_bind_double(bodyQuery.stmt, 8, initialAngle);
    sqlite3_bind_double(bodyQuery.stmt, 9, radius);
    sqlite3_bind_text(bodyQuery.stmt, 10, colorText.c_str(), -1, SQLITE_TRANSIENT);

    if (!bodyQuery.step("SaveGame: Failed to insert body record"))
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

    SQLiteQuery facilityQuery(loader, "INSERT INTO facility (id, system_id, location_id, type) VALUES (?, ?, ?, ?);");
    if (!facilityQuery.stmt)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to prepare facility insert for base");
        return -9;
    }

    int rc = sqlite3_bind_int(facilityQuery.stmt, 1, facilityId);
    if (rc != SQLITE_OK)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to bind facility id for base");
        return -10;
    }

    rc = sqlite3_bind_int(facilityQuery.stmt, 2, rf->location->system->id);
    if (rc != SQLITE_OK)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to bind system id for base");
        return -11;
    }

    rc = sqlite3_bind_int(facilityQuery.stmt, 3, rf->location->id);
    if (rc != SQLITE_OK)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to bind location id for base");
        return -12;
    }

    rc = sqlite3_bind_int(facilityQuery.stmt, 4, SLOC_SURFACE);
    if (rc != SQLITE_OK)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to bind facility type for base");
        return -13;
    }

    if (!facilityQuery.step("SaveGame: Failed to execute facility insert for base"))
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

    SQLiteQuery facilityQuery(loader, "INSERT INTO facility (id, system_id, location_id, type) VALUES (?, ?, ?, ?);");
    if (!facilityQuery.stmt)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to prepare facility insert for orbital");
        return -9;
    }

    int rc = sqlite3_bind_int(facilityQuery.stmt, 1, facilityId);
    if (rc != SQLITE_OK)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to bind facility id for orbital");
        return -10;
    }

    rc = sqlite3_bind_int(facilityQuery.stmt, 2, orbital->location->system->id);
    if (rc != SQLITE_OK)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to bind system id for orbital");
        return -11;
    }

    rc = sqlite3_bind_int(facilityQuery.stmt, 3, orbital->location->id);
    if (rc != SQLITE_OK)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to bind location id for orbital");
        return -12;
    }

    rc = sqlite3_bind_int(facilityQuery.stmt, 4, SLOC_ORBIT);
    if (rc != SQLITE_OK)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to bind facility type for orbital");
        return -13;
    }

    if (!facilityQuery.step("SaveGame: Failed to execute facility insert for orbital"))
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
        uint32_t amount = stores->resources[resourceId];
        if (amount == 0)
        {
            continue;
        }

        sqlite3_reset(storesQuery.stmt);
        sqlite3_clear_bindings(storesQuery.stmt);

        int rc = sqlite3_bind_int(storesQuery.stmt, 1, facilityId);
        if (rc != SQLITE_OK)
        {
            TraceLog(LOG_ERROR, "SaveGame: Failed to bind facility id for store");
            return -10;
        }

        rc = sqlite3_bind_int(storesQuery.stmt, 2, resourceId);
        if (rc != SQLITE_OK)
        {
            TraceLog(LOG_ERROR, "SaveGame: Failed to bind resource id for store");
            return -11;
        }

        rc = sqlite3_bind_int(storesQuery.stmt, 3, amount);
        if (rc != SQLITE_OK)
        {
            TraceLog(LOG_ERROR, "SaveGame: Failed to bind store amount");
            return -12;
        }

        if (!storesQuery.step("SaveGame: Failed to execute stores insert"))
        {
            return -13;
        }
    }

    return 0;
}
