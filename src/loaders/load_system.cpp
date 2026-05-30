#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>

#include "loaders/load_system.h"
#include "state/game.h"

// loader to read system data from SQLite database and populate System struct

Color ColorFromHexStr(const char *hex)
{
    // Skip '#' if the string happens to include it
    if (hex[0] == '#')
        hex++;

    unsigned int r, g, b, a;

    auto length = std::strlen(hex);

    // sscanf returns the number of items successfully filled
    // We expect 4 items for RRGGBBAA
    if (length == 6)
    {
        // If we have 6 characters, assume it's RRGGBB and set alpha to 255
        if (sscanf(hex, "%02x%02x%02x", &r, &g, &b) != 3)
        {
            // Return blank or magenta if parsing fails to alert you
            return MAGENTA;
        }
        a = 255; // default alpha
    }
    else if (length == 8)
    {
        // If we have 8 characters, parse RRGGBBAA
        if (sscanf(hex, "%02x%02x%02x%02x", &r, &g, &b, &a) != 4)
        {
            // Return blank or magenta if parsing fails to alert you
            return MAGENTA;
        }
    }
    else
    {
        // Invalid length, return magenta to indicate error
        TraceLog(LOG_ERROR, "Invalid color string length: %s", hex);
        return MAGENTA;
    }

    return (Color){(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a};
}

int queryInt(Loader *loader, const char *sql, int param)
{
    SQLiteQuery q(loader, sql);

    q.bind(param);

    if (q.next())
    {
        return sqlite3_column_int(q, 0);
    }
    TraceLog(LOG_ERROR, "Failed to execute query: %s", sqlite3_errmsg(loader->db));
    return -1;
}

int countChildBodies(Loader *loader, int primary_id)
{
    return queryInt(loader, "SELECT COUNT(*) FROM bodies WHERE primary_id = ?", primary_id);
}

int countSystemBodies(Loader *loader, int system_id)
{
    return queryInt(loader, "SELECT COUNT(*) FROM bodies WHERE system_id = ?", system_id);
}

int getPrimaryBodyId(Loader *loader, int system_id)
{
    return queryInt(loader, "SELECT id FROM bodies WHERE system_id = ? AND primary_id = 0 LIMIT 1", system_id);
}

bool Loader::loadBodies()
{
    SQLiteQuery query(this, "SELECT id, primary_id, name, type, orbital_radius, orbital_velocity, initial_angle, radius, color, system_id FROM bodies ORDER BY id");
    // presize system arrays
    // counters for index
    int systemIndex[10] = {0}; // track current index for each system

    for (auto sys : systems)
    {
        int primary_id = getPrimaryBodyId(this, sys->id);
        sys->setNumBodies(countSystemBodies(this, sys->id));
        TraceLog(LOG_INFO, "Loading system %d with primary body ID %d and %d bodies", sys->id, primary_id, sys->numPlanets);
    }

    // planet primary _index_ is mapped version of planet ID in array of data for that system

    while (query.next())
    {
        int id = sqlite3_column_int(query.stmt, 0);
        int local_primary_id = sqlite3_column_int(query.stmt, 1);
        const char *name = (const char *)sqlite3_column_text(query.stmt, 2);
        int type = sqlite3_column_int(query.stmt, 3);
        float orbital_radius = sqlite3_column_double(query.stmt, 4);
        float orbital_velocity = sqlite3_column_double(query.stmt, 5);
        float initial_angle = sqlite3_column_double(query.stmt, 6);
        float radius = sqlite3_column_double(query.stmt, 7);
        const char *color_str = (const char *)sqlite3_column_text(query.stmt, 8);
        int system_id = sqlite3_column_int(query.stmt, 9);

        System *system = systems[system_id];

        Color color;
        if (color_str != NULL && strlen(color_str) > 0)
        {
            color = ColorFromHexStr(color_str);
        }
        else
        {
            color = WHITE;
        }

        // populate system arrays
        int index = systemIndex[system_id]++;
        system->planetDistances[index] = orbital_radius;
        system->planetSizes[index] = radius;
        system->planetColors[index] = color;
        system->planetVelocities[index] = orbital_velocity;
        system->planetInitialAngles[index] = initial_angle;
        system->planetPositions[index] = (Vector2){orbital_radius * cosf(initial_angle), orbital_radius * sinf(initial_angle)};
        system->planetPrimaryIndexes[index] = local_primary_id;

        auto location = game->createLocation(system, id, name, LocationType(type));
        location->primary_id = local_primary_id;
        location->index = index; // set array index for this location
        location->system_id = system_id;
    }

    // Now convert primary_id in planetPrimaryIndexes to array index
    // simply index constructed for system, from location array entry
    for (auto &loc : game->allLocations())
    {
        System *system = systems[loc->system_id];
        if (loc->system_id == 0)
        {
            // generic space
            continue;
        }
        if (loc->primary_id == 0)
        {
            // primary body, set to -1
            system->planetPrimaryIndexes[loc->index] = -1;
            system->primary = loc.get(); // set primary location for system
        }
        else
        {
            auto primary_loc = game->locationByID(loc->primary_id);
            if (primary_loc)
            {
                system->planetPrimaryIndexes[loc->index] = primary_loc->index;
                primary_loc->children.push_back(loc.get()); // build location hierarchy based on primary_id relationships
            }
            else
            {
                TraceLog(LOG_ERROR, "Could not find primary location with ID %d for location %s", loc->primary_id, loc->name);
            }
        }
    }

    // now system->planetPrimaryIndexes contains the array index of the primary body for each planet, or -1 if it's a primary body itself

    // iterate locations and read resource availability for each from body_resources table

    SQLiteQuery resourceQuery(this, "SELECT body_id, resource_id, availability FROM body_resources");

    while (resourceQuery.next())
    {
        int body_id = sqlite3_column_int(resourceQuery, 0);
        auto loc = game->locationByID(body_id);
        if (!loc)
        {
            TraceLog(LOG_ERROR, "Could not find location with ID %d for body_resources entry", body_id);
            continue;
        }
        int resource_id = sqlite3_column_int(resourceQuery, 1);
        int availability = sqlite3_column_int(resourceQuery, 2);
        if (resource_id >= 0 && resource_id < ResourceType::Count)
        {
            loc->resources.availability[resource_id] = availability;
        }
        else
        {
            TraceLog(LOG_ERROR, "Invalid resource_id %d for body %d", resource_id, loc->id);
        }
    }

    return true;
}

bool Loader::loadSystems()
{
    // define location 0 as space
    auto space_location = game->createLocation(nullptr, 0, "Space", LOCATION_TYPE_SPACE);

    // set up system 0 for tests
    auto system0 = game->createSystem(0, "Test System");
    systems.push_back(system0);
    // query systems table and populate Game instance
    {
        SQLiteQuery query(this, "SELECT id, name FROM systems ORDER BY id");

        while (query.next())
        {
            int id = sqlite3_column_int(query.stmt, 0);
            const char *name = (const char *)sqlite3_column_text(query.stmt, 1);
            auto system = game->createSystem(id, name);
            systems.push_back(system);
        }
    }

    // load all bodies
    loadBodies();

    return true;
}