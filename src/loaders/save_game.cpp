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
}

// Save game implementation

// This will write to a new SQLite file with the same structure as read by e.g. load_system

/*
Schema:

CREATE TABLE bodies ( id INTEGER PRIMARY key, system_id INT, primary_id INT, type INT, name text, orbital_radius float, orbital_velocity float, initial_angle float, radius float, color text )
CREATE TABLE systems ( id INTEGER primary key, name text )
CREATE TABLE facilities ( id int, system_id int, location_id int, type int )
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
        "CREATE TABLE IF NOT EXISTS bodies ( id INTEGER PRIMARY KEY, system_id INT, primary_id INT, type INT, name TEXT, orbital_radius FLOAT, orbital_velocity FLOAT, initial_angle FLOAT, radius FLOAT, color TEXT );"
        "CREATE TABLE IF NOT EXISTS systems ( id INTEGER PRIMARY KEY, name TEXT );"
        "CREATE TABLE IF NOT EXISTS facilities ( id INT, system_id INT, location_id INT, type INT );"
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
    std::snprintf(sql, sizeof(sql), "DELETE FROM game;");
    int rc = sqlite3_exec(loader->db, sql, nullptr, nullptr, errorMessage);
    if (rc != SQLITE_OK)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to clear game table: %s", errorMessage.get() ? errorMessage.get() : "Unknown error");
        return -5;
    }

    std::snprintf(sql, sizeof(sql), "INSERT INTO game (game_time) VALUES (%u);", game->game_time);
    rc = sqlite3_exec(loader->db, sql, nullptr, nullptr, errorMessage);
    if (rc != SQLITE_OK)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to insert game_time: %s", errorMessage.get() ? errorMessage.get() : "Unknown error");
        return -6;
    }

        // TODO: Iterate through all systems in game and save them
    // TODO: Save all bases (resource facilities)
    // TODO: Save all orbitals

    return 0; // TODO: Implement full game save logic
}

int SaveGame::saveSystem(System *system)
{
    // Validate input
    if (!system)
    {
        TraceLog(LOG_ERROR, "SaveGame: Null system pointer");
        return -4;
    }

    // TODO: Save system name to systems table
    // TODO: Iterate through all locations in system and call saveLocation
    // TODO: Map location properties to bodies table

    return 0; // TODO: Implement full system save logic
}

int SaveGame::saveLocation(Location *location)
{
    // Validate input
    if (!location)
    {
        TraceLog(LOG_ERROR, "SaveGame: Null location pointer");
        return -5;
    }

    // TODO: Save location data to bodies table including:
    // - ID and primary_id
    // - Name and type
    // - Orbital properties (radius, velocity, angle)
    // - Physical properties (radius, color)

    return 0; // TODO: Implement full location save logic
}

int SaveGame::saveBase(ResourceFacility *rf)
{
    // Validate input
    if (!rf)
    {
        TraceLog(LOG_ERROR, "SaveGame: Null resource facility pointer");
        return -6;
    }

    // TODO: Save resource facility to facilities table including:
    // - Facility ID
    // - System ID (from location->system)
    // - Location ID (from location->id)
    // - Type (0 for resource facility)
    // TODO: Save associated stores by calling saveStores

    return 0; // TODO: Implement full resource facility save logic
}

int SaveGame::saveOrbital(Orbital *orbital)
{
    // Validate input
    if (!orbital)
    {
        TraceLog(LOG_ERROR, "SaveGame: Null orbital pointer");
        return -7;
    }

    // TODO: Save orbital to facilities table including:
    // - Facility ID
    // - System ID (from location->system)
    // - Location ID (from location->id)
    // - Type (1 for orbital)
    // TODO: Save associated stores by calling saveStores
    // TODO: Save factory data if present

    return 0; // TODO: Implement full orbital save logic
}

int SaveGame::saveStores(Stores *stores)
{
    // Validate input
    if (!stores)
    {
        TraceLog(LOG_ERROR, "SaveGame: Null stores pointer");
        return -8;
    }

    // TODO: Save all resources from stores to stores table:
    // - Iterate through ResourceType::Count
    // - For each resource with non-zero amount:
    //   - Insert row: (facility_id, resource_id, amount)

    return 0; // TODO: Implement full stores save logic
}