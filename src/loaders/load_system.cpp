#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>

#include "loaders/load_system.h"

// loader to read system data from SQLite database and populate System struct

Color ColorFromHexStr(const char *hex) {
    // Skip '#' if the string happens to include it
    if (hex[0] == '#') hex++;

    unsigned int r, g, b, a;
    
    // sscanf returns the number of items successfully filled
    // We expect 4 items for RRGGBBAA
    if (sscanf(hex, "%02x%02x%02x%02x", &r, &g, &b, &a) != 4) {
        // Return blank or magenta if parsing fails to alert you
        return MAGENTA; 
    }

    return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a };
}

int queryInt(Loader* loader, const char* sql, int param) {
    SQLiteQuery q(loader, sql);
    
    q.bind(param);
    
    if (q.next()) {
        return sqlite3_column_int(q, 0);
    } 
    TraceLog(LOG_ERROR, "Failed to execute query: %s", sqlite3_errmsg(loader->db));
    return -1;
}

int countChildBodies(Loader* loader, int primary_id) {
    return queryInt(loader, "SELECT COUNT(*) FROM bodies WHERE primary_id = ?", primary_id);    
}

int countSystemBodies(Loader* loader, int system_id) {
    return queryInt(loader, "SELECT COUNT(*) FROM bodies WHERE system_id = ?", system_id);    
}

int getPrimaryBodyId(Loader* loader, int system_id) {
    return queryInt(loader, "SELECT id FROM bodies WHERE system_id = ? AND primary_id = 0 LIMIT 1", system_id);
}

std::string queryString(Loader* loader, const char* sql, int param) {
    std::string result = "";

    SQLiteQuery q(loader, sql);
    
    q.bind(param);
    
    if (q.next()) {
        const unsigned char* text = sqlite3_column_text(q, 0);
        if (text) {
            result = std::string(reinterpret_cast<const char*>(text));
        }
    } 
    TraceLog(LOG_ERROR, "Failed to execute query: %s", sqlite3_errmsg(loader->db));
    return result;
}

bool loadSystem(Loader* loader, int system_id, System* system) {

    // system name
    system->name = queryString(loader, "SELECT name FROM systems WHERE id = ?", system_id);

    // get ID of primary body (e.g. the star) for this system
    int primary_id = getPrimaryBodyId(loader, system_id);
    if (primary_id == -1) {
        TraceLog(LOG_ERROR, "Failed to get primary body ID for system %d", system_id);
        return false;
    }

    // get number of planets in system

    system->setNumBodies(countSystemBodies(loader, system_id));

    TraceLog(LOG_INFO, "Loading system %d with primary body ID %d and %d bodies", system_id, primary_id, system->numPlanets);    

    SQLiteQuery query(loader, "SELECT id, primary_id, name, type, orbital_radius, orbital_velocity, initial_angle, radius, color FROM bodies WHERE system_id = ? ORDER BY id");
    
    sqlite3_bind_int(query.stmt, 1, system_id);

    // need to map primary_id to array index for that ID - build a simple lookup table first
    int* idToIndex = (int*)malloc(sizeof(int) * system->numPlanets * 2); // pairs of (id, index)    
    if (!idToIndex) {
        TraceLog(LOG_ERROR, "Failed to allocate ID to index mapping");
        return false;
    }

    int index = 0;
    while (query.next()) {
        int id = sqlite3_column_int(query.stmt, 0);
        int local_primary_id = sqlite3_column_int(query.stmt, 1);
        const char* name = (const char*)sqlite3_column_text(query.stmt, 2);
        int type = sqlite3_column_int(query.stmt, 3);
        float orbital_radius = sqlite3_column_double(query.stmt, 4);
        float orbital_velocity = sqlite3_column_double(query.stmt, 5);
        float initial_angle = sqlite3_column_double(query.stmt, 6);
        float radius = sqlite3_column_double(query.stmt, 7);
        const char* color_str = (const char*)sqlite3_column_text(query.stmt, 8);

        Color color;
        if (color_str != NULL && strlen(color_str) > 0) {
            color = ColorFromHexStr(color_str);
        } else {
            color = WHITE;
        }

        // populate system arrays
        system->planetDistances[index] = orbital_radius;
        system->planetSizes[index] = radius;
        system->planetColors[index] = color;
        system->planetVelocities[index] = orbital_velocity;
        system->planetPositions[index] = (Vector2){ orbital_radius * cosf(initial_angle), orbital_radius * sinf(initial_angle) };
        system->planetPrimaryIndexes[index] = local_primary_id;

        auto location = system->addLocation(name, LocationType(type));
        location->id = id;
        location->primary_id = local_primary_id;

        // add to ID to index mapping
        idToIndex[index * 2] = id;
        idToIndex[index * 2 + 1] = index;

        index++;
    }

    // Now convert primary_id in planetPrimaryIndexes to array index using our lookup table
    for (int i = 0; i < system->numPlanets; i++) {
        int local_primary_id = system->planetPrimaryIndexes[i];
        if (local_primary_id == 0) {
            system->planetPrimaryIndexes[i] = -1; // mark primary bodies with -1
        } else {
            // lookup local_primary_id in idToIndex mapping
            int primary_index = -1;
            for (int j = 0; j < system->numPlanets; j++) {
                if (idToIndex[j * 2] == local_primary_id) {
                    primary_index = idToIndex[j * 2 + 1];
                    break;
                }
            }
            system->planetPrimaryIndexes[i] = primary_index;
        }
    }
    // now system->planetPrimaryIndexes contains the array index of the primary body for each planet, or -1 if it's a primary body itself    

    free(idToIndex);

    // iterate Location collection, and if there is a primary_id, find the corresponding Location and add child
    for (const auto& loc : system->locations) {
        if (loc->primary_id != 0) {
            // find parent location
            Location* parent = nullptr;
            for (const auto& potential_parent : system->locations) {
                if (potential_parent->id == loc->primary_id) {
                    parent = potential_parent.get();
                    break;
                }
            }
            if (parent) {
                parent->children.push_back(loc.get());
            } else {
                TraceLog(LOG_WARNING, "Could not find parent location with ID %d for location %s", loc->primary_id, loc->name.c_str());
            }
        }
    } // parent building

    return true;
}