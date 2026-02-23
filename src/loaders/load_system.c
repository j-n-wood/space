#include <stdlib.h>
#include <stdio.h>

#include "loaders/load_system.h"

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

int countChildBodies(Loader* loader, int primary_id) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT COUNT(*) FROM bodies WHERE primary_id = ?";
    if (sqlite3_prepare_v2(loader->db, sql, -1, &stmt, 0) != SQLITE_OK) {
        TraceLog(LOG_ERROR, "Failed to prepare count query: %s", sqlite3_errmsg(loader->db));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, primary_id);

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    } else {
        TraceLog(LOG_ERROR, "Failed to execute count query: %s", sqlite3_errmsg(loader->db));
    }

    sqlite3_finalize(stmt);
    return count;
}

bool loadSystem(Loader* loader, int system_id, System* system) {

    // get number of planets in system
    system->numPlanets = countChildBodies(loader, 1); // assuming primary_id = 1 (convention)

    // allocate arrays based on number of planets
    system->planetDistances = malloc(sizeof(float) * system->numPlanets);
    system->planetSizes = malloc(sizeof(float) * system->numPlanets);
    system->planetColors = malloc(sizeof(Color) * system->numPlanets);
    system->planetVelocities = malloc(sizeof(float) * system->numPlanets);
    system->planetPositions = malloc(sizeof(Vector2) * system->numPlanets);

    if (!system->planetDistances || !system->planetSizes || !system->planetColors || 
        !system->planetVelocities || !system->planetPositions) {
        TraceLog(LOG_ERROR, "Failed to allocate system arrays");
        return false;
    }

    sqlite3_stmt *stmt;

    const char *sql = "SELECT id, primary_id, name, type, orbital_radius, orbital_velocity, initial_angle, radius, color FROM bodies WHERE system_id = ? ORDER BY id";
    if (sqlite3_prepare_v2(loader->db, sql, -1, &stmt, 0) != SQLITE_OK) {
        TraceLog(LOG_ERROR, "Failed to fetch data: %s", sqlite3_errmsg(loader->db));
        return false;
    }

    sqlite3_bind_int(stmt, 1, system_id);

    int index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        int primary_id = sqlite3_column_int(stmt, 1);
        const char* name = (const char*)sqlite3_column_text(stmt, 2);
        int type = sqlite3_column_int(stmt, 3);
        float orbital_radius = sqlite3_column_double(stmt, 4);
        float orbital_velocity = sqlite3_column_double(stmt, 5);
        float initial_angle = sqlite3_column_double(stmt, 6);
        float radius = sqlite3_column_double(stmt, 7);
        const char* color_str = (const char*)sqlite3_column_text(stmt, 8);

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

        index++;
    }

    sqlite3_finalize(stmt);

    return true;
}