#include "loaders/save_game.h"
#include "loaders/loader.h"
#include "state/game.h"
#include "state/location.h"
#include "state/system.h"
#include "state/resourceFacility.h"
#include "state/orbital.h"
#include "state/stores.h"
#include "state/resources.h"
#include "state/autopilot.h"

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
CREATE TABLE items ( id int, name text, description text, tool int, researched int, tech_level int, orbital int, mass int, production_time float, doc_image_index int, production_image_index int, pod_capacity int);
CREATE TABLE item_build_requirements ( item_id int, resource_id int, amount int);
CREATE TABLE research_topics ( id int, name text, description text, required_time float, progress float, available int);
CREATE TABLE research_topic_unlocks_items ( topic_id int, item_id int);
CREATE TABLE research_topic_unlocks_topics ( topic_id int, unlocks_topic_id int);
CREATE TABLE body_resources ( body_id int, resource_id int, availability int );
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
        "CREATE TABLE IF NOT EXISTS items ( id int, name text, description text, tool int, researched int, tech_level int, orbital int, mass int, production_time float, doc_image_index int, production_image_index int, pod_capacity int);"
        "CREATE TABLE IF NOT EXISTS item_build_requirements ( item_id int, resource_id int, amount int);"
        "CREATE TABLE IF NOT EXISTS research_topics ( id int, name text, description text, required_time float, progress float, available int);"
        "CREATE TABLE IF NOT EXISTS research_topic_unlocks_items ( topic_id int, item_id int);"
        "CREATE TABLE IF NOT EXISTS research_topic_unlocks_topics ( topic_id int, unlocks_topic_id int);"
        "CREATE TABLE IF NOT EXISTS body_resources ( body_id int, resource_id int, availability int );"
        "CREATE TABLE IF NOT EXISTS craft ( id int, name text, type int, state int, state_timer float, total_state_timer float, location_id int, fuel int, max_pods int, drive int, destination_index int );"
        "CREATE TABLE IF NOT EXISTS craft_pods ( craft_id int, pod_index int, type int, content_type int, amount int );"
        "CREATE TABLE IF NOT EXISTS craft_destinations ( craft_id int, destination_index int, system_id int, location_id int, sublocation int, docked int );"
        "CREATE TABLE IF NOT EXISTS craft_autopilot ( craft_id int, state int );"
        "CREATE TABLE IF NOT EXISTS craft_autopilot_flows ( craft_id int, resource_index int, flow_flags int );"
        "CREATE TABLE IF NOT EXISTS craft_autopilot_cursors ( craft_id int, endpoint_index int, cursor_position int );"
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

    if (saveResearchTopics(game) != 0)
    {
        return -11;
    }

    if (saveCraft(game) != 0)
    {
        return -12;
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

    const char *storeSql =
        "INSERT INTO body_resources ( body_id, resource_id, availability ) "
        "VALUES (?, ?, ?);";
    SQLiteQuery storeQuery(loader, storeSql);
    if (!storeQuery.stmt)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to prepare body_resources insert");
        return -11;
    }

    size_t bodyCount = system->locations.size();
    for (size_t idx = 0; idx < bodyCount; ++idx)
    {
        int result = saveLocation(bodyQuery, system, idx, systemId);
        if (result != 0)
        {
            return result;
        }

        // save body resources
        Location *loc = system->locations[idx].get();
        if (!loc)
        {
            continue;
        }
        for (int resource_id = 0; resource_id < ResourceType::Count; ++resource_id)
        {
            int availability = loc->resources.availability[resource_id];
            if (availability == 0)
            {
                continue; // skip unavailable resources
            }

            sqlite3_reset(storeQuery.stmt);
            sqlite3_clear_bindings(storeQuery.stmt);

            if (!storeQuery.bind(1, loc->id)
                     .bind(2, resource_id)
                     .bind(3, availability)
                     .step("SaveGame: Failed to insert body_resources record"))
            {
                return -12;
            }
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
    const float initialAngle = locationIndex < system->planetPositions.size() ? system->planetInitialAngles[locationIndex] : 0.0f;
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

    SublocationType sublocation = SLOC_SURFACE;
    if (rf->training_facility || rf->research_facility) // somewhat hacky //TODO
    {
        sublocation = SLOC_EARTH_CITY;
    }

    if (!facilityQuery.bind(1, facilityId)
             .bind(2, rf->location->system->id)
             .bind(3, rf->location->id)
             .bind(4, sublocation)
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
        SQLiteQuery itemQ(loader, "INSERT INTO items (id, name, description, tool, researched, tech_level, orbital, mass, production_time, doc_image_index, production_image_index, pod_capacity) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
        for (auto &item : game->items)
        {
            sqlite3_reset(itemQ.stmt);
            sqlite3_clear_bindings(itemQ.stmt);
            if (!itemQ.bind(1, idx).bind(2, item.name).bind(3, item.description).bind(4, item.tool).bind(5, item.researched).bind(6, item.tech_level).bind(7, item.orbital).bind(8, item.mass).bind(9, item.production_time).bind(10, item.doc_image_index).bind(11, item.production_image_index).bind(12, item.pod_capacity).step("SaveGame: Failed to save item"))
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

int SaveGame::saveResearchTopics(Game *game)
{
    // Similar to saveItems, but for research topics
    int idx{0};

    struct ResearchTopicUnlock
    {
        int topic_id;
        int item_id;
    };

    std::vector<ResearchTopicUnlock> item_unlocks;
    std::vector<ResearchTopicUnlock> topic_unlocks;
    {
        SQLiteQuery topicQ(loader, "INSERT INTO research_topics (id, name, description, required_time, progress, available) VALUES (?, ?, ?, ?, ?, ?);");
        for (auto &topic : game->researchTopics)
        {
            sqlite3_reset(topicQ.stmt);
            sqlite3_clear_bindings(topicQ.stmt);
            if (!topicQ.bind(1, idx).bind(2, topic.name).bind(3, topic.description).bind(4, topic.requiredTime).bind(5, topic.progress).bind(6, topic.available).step("SaveGame: Failed to save research topic"))
            {
                return -17;
            }

            for (auto &itemId : topic.unlocksItems)
            {
                item_unlocks.emplace_back(ResearchTopicUnlock{idx, itemId});
            }
            for (auto &topicId : topic.unlocksTopics)
            {
                topic_unlocks.emplace_back(ResearchTopicUnlock{idx, topicId});
            }

            ++idx;
        }
    }

    {
        SQLiteQuery unlockQ(loader, "INSERT INTO research_topic_unlocks_items (topic_id, item_id) VALUES (?, ?)");
        for (auto &unlock : item_unlocks)
        {
            sqlite3_reset(unlockQ.stmt);
            sqlite3_clear_bindings(unlockQ.stmt);
            if (!unlockQ.bind(1, unlock.topic_id).bind(2, unlock.item_id).step("SaveGame: Failed to save research topic unlock"))
            {
                return -18;
            }
        }
    }

    {
        SQLiteQuery unlockQ(loader, "INSERT INTO research_topic_unlocks_topics (topic_id, unlocks_topic_id) VALUES (?, ?)");
        for (auto &unlock : topic_unlocks)
        {
            sqlite3_reset(unlockQ.stmt);
            sqlite3_clear_bindings(unlockQ.stmt);
            if (!unlockQ.bind(1, unlock.topic_id).bind(2, unlock.item_id).step("SaveGame: Failed to save research topic unlock"))
            {
                return -19;
            }
        }
    }
    return 0;
}

int SaveGame::saveCraft(Game *game)
{
    // Similar to saveBase/saveOrbital, but for shuttles and IOS
    // This is a bit more complex as we need to save the craft itself and also its pod contents, destinations
    // and autopilot state

    int craftId = 1; // start from 1, as 0 can be used to indicate no craft in some cases

    for (auto &craft : game->allShuttles())
    {
        if (craft && saveCraft(craft, craftId++) != 0)
        {
            return -20;
        }
    }

    for (auto &craft : game->allIOS())
    {
        if (craft && saveCraft(craft.get(), craftId++) != 0)
        {
            return -21;
        }
    }

    return 0;
}

int SaveGame::saveCraft(Craft *craft, int craftId)
{
    if (!loader || !loader->db)
    {
        TraceLog(LOG_ERROR, "SaveGame: Null loader pointer");
        return -6;
    }

    if (!craft)
    {
        TraceLog(LOG_ERROR, "SaveGame: Null craft pointer");
        return -7;
    }

    SQLiteQuery craftQuery(loader, "INSERT INTO craft (id, name, type, state, state_timer, total_state_timer, location_id, fuel, max_pods, drive, destination_index) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
    if (!craftQuery.stmt)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to prepare craft insert");
        return -9;
    }

    int locationId = craft->location ? craft->location->id : 0;

    if (!craftQuery.bind(1, craftId)
             .bind(2, craft->name)
             .bind(3, static_cast<int>(craft->type))
             .bind(4, static_cast<int>(craft->state))
             .bind(5, craft->state_timer)
             .bind(6, craft->total_state_timer)
             .bind(7, locationId)
             .bind(8, craft->fuel)
             .bind(9, craft->max_pods)
             .bind(10, craft->drive)
             .bind(11, craft->destination_index)
             .step("SaveGame: Failed to execute craft insert"))
    {
        return -14;
    }

    // save pods
    SQLiteQuery podQuery(loader, "INSERT INTO craft_pods (craft_id, pod_index, type, content_type, amount) VALUES (?, ?, ?, ?, ?);");
    if (!podQuery.stmt)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to prepare craft_pods insert");
        return -9;
    }
    for (int podIndex = 0; podIndex < craft->max_pods; ++podIndex)
    {
        const Pod &pod = craft->pods[podIndex];

        sqlite3_reset(podQuery.stmt);
        sqlite3_clear_bindings(podQuery.stmt);

        if (!podQuery.bind(1, craftId)
                 .bind(2, podIndex)
                 .bind(3, static_cast<int>(pod.type))
                 .bind(4, pod.contentType)
                 .bind(5, pod.amount)
                 .step("SaveGame: Failed to execute craft_pods insert"))
        {
            return -14;
        }
    }

    if (saveCraftDestinations(craft, craftId) != 0)
    {
        return -16;
    }
    if (saveAutopilot(craft, craftId) != 0)
    {
        return -17;
    }
    return 0;
}

int SaveGame::saveCraftDestinations(Craft *craft, int craftId)
{
    // Save the craft's destinations to the craft_destinations table
    // This will include the system, location, sublocation, and docked state for each destination

    for (uint8_t i = 0; i < MAX_DESTINATIONS; ++i)
    {
        const Endpoint &dest = craft->destinations[i];
        int systemId = dest.location && dest.location->system ? dest.location->system->id : 0;
        int locationId = dest.location ? dest.location->id : 0;

        SQLiteQuery destQuery(loader, "INSERT INTO craft_destinations (craft_id, destination_index, system_id, location_id, sublocation, docked) VALUES (?, ?, ?, ?, ?, ?);");
        if (!destQuery.stmt)
        {
            TraceLog(LOG_ERROR, "SaveGame: Failed to prepare craft_destinations insert");
            return -9;
        }

        if (!destQuery.bind(1, craftId)
                 .bind(2, i)
                 .bind(3, systemId)
                 .bind(4, locationId)
                 .bind(5, static_cast<int>(dest.sublocation))
                 .bind(6, dest.docked)
                 .step("SaveGame: Failed to execute craft_destinations insert"))
        {
            return -14;
        }
    }

    return 0;
}

int SaveGame::saveAutopilot(Craft *craft, int craftId)
{
    // Save autopilot state for the given craft
    // This will include the autopilot state, resource flows, and cursor positions

    if (!craft->autopilot)
    {
        return 0; // no autopilot to save
    }

    // save state
    SQLiteQuery apQuery(loader, "INSERT INTO craft_autopilot (craft_id, state) VALUES (?, ?);");
    if (!apQuery.stmt)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to prepare craft_autopilot insert");
        return -9;
    }
    if (!apQuery.bind(1, craftId)
             .bind(2, static_cast<int>(craft->autopilot->state))
             .step("SaveGame: Failed to execute craft_autopilot insert"))
    {
        return -14;
    }

    // flows
    SQLiteQuery flowQuery(loader, "INSERT INTO craft_autopilot_flows (craft_id, resource_index, flow_flags) VALUES (?, ?, ?);");
    if (!flowQuery.stmt)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to prepare craft_autopilot_flows insert");
        return -9;
    }
    for (int i = 0; i < ResourceType::Count; ++i)
    {
        int flowFlags = craft->autopilot->flow[i];
        if (flowFlags == 0)
        {
            continue; // skip inactive flows
        }
        // reset statement for next execution
        sqlite3_reset(flowQuery.stmt);
        sqlite3_clear_bindings(flowQuery.stmt);
        if (!flowQuery.bind(1, craftId)
                 .bind(2, i)
                 .bind(3, flowFlags)
                 .step("SaveGame: Failed to execute craft_autopilot_flows insert"))
        {
            return -14;
        }
    }

    // cursors
    SQLiteQuery cursorQuery(loader, "INSERT INTO craft_autopilot_cursors (craft_id, endpoint_index, cursor_position) VALUES (?, ?, ?);");
    if (!cursorQuery.stmt)
    {
        TraceLog(LOG_ERROR, "SaveGame: Failed to prepare craft_autopilot_cursors insert");
        return -9;
    }
    for (int endpoint = 0; endpoint < 2; ++endpoint)
    {
        uint8_t cursorPos = craft->autopilot->cursors[endpoint];
        sqlite3_reset(cursorQuery.stmt);
        sqlite3_clear_bindings(cursorQuery.stmt);
        if (!cursorQuery.bind(1, craftId)
                 .bind(2, endpoint)
                 .bind(3, cursorPos)
                 .step("SaveGame: Failed to execute craft_autopilot_cursors insert"))
        {
            return -14;
        }
    }

    return 0;
}