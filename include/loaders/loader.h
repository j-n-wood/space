#pragma once

// handle wrapping SQLite handles and queries

#include "sqlite3.h"
#include <string>

extern "C"
{
#include "raylib.h" // logger
}

class Game;
class Location;

/// Lightweight SQLite save loader.
/// Wraps a SQLite database handle and provides the entry point for load operations.
class Loader
{
    Game *game;

public:
    explicit Loader(const char *dbPath);
    ~Loader();

    bool isValid() const { return db != nullptr; }

    sqlite3 *db;

    void setGame(Game *g) { game = g; }

    /// Load all systems from the opened database into the current game.
    bool loadSystems();

    /// Load game state (e.g., game_time) from the database.
    bool loadGame();

    /// Load all facilities (bases and orbitals) and attach them to their locations.
    bool loadFacilities();

    /// Load stores (resource inventories) for all facilities.
    bool loadStores();

private:
    Location *findLocation(int system_id, int location_id);
};

/// Scoped SQLite statement wrapper.
/// Supports chained bind(...) calls and step() to execute prepared statements cleanly.
class SQLiteQuery
{
public:
    sqlite3 *db;
    sqlite3_stmt *stmt;
    bool valid;

    SQLiteQuery(Loader *loader, const char *sql) : db(loader->db), stmt(nullptr), valid(false)
    {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
        {
            TraceLog(LOG_ERROR, "Failed to prepare query: %s", sqlite3_errmsg(db));
        }
        valid = true;
    }

    ~SQLiteQuery()
    {
        if (stmt)
        {
            sqlite3_finalize(stmt);
        }
    }

    bool next()
    {
        if ((!stmt) || (!valid))
            return false;
        return sqlite3_step(stmt) == SQLITE_ROW;
    }

    bool step(const char *message)
    {
        if ((!stmt) || (!valid))
            return false;

        int result = sqlite3_step(stmt);
        if (result != SQLITE_DONE)
        {
            TraceLog(LOG_ERROR, "%s: %s", message, sqlite3_errmsg(db));
            return false;
        }
        return true;
    }

    bool step()
    {
        return step("SQLiteQuery: Failed to execute statement");
    }

    SQLiteQuery &bind(int index, int value)
    {
        if (!stmt || !valid)
            return *this;

        if (sqlite3_bind_int(stmt, index, value) != SQLITE_OK)
        {
            TraceLog(LOG_ERROR, "SQLiteQuery: Failed to bind int at index %d: %s", index, sqlite3_errmsg(db));
            valid = false;
        }
        return *this;
    }

    SQLiteQuery &bind(int index, sqlite3_int64 value)
    {
        if (!stmt || !valid)
            return *this;

        if (sqlite3_bind_int64(stmt, index, value) != SQLITE_OK)
        {
            TraceLog(LOG_ERROR, "SQLiteQuery: Failed to bind int64 at index %d: %s", index, sqlite3_errmsg(db));
            valid = false;
        }
        return *this;
    }

    SQLiteQuery &bind(int index, double value)
    {
        if (!stmt || !valid)
            return *this;

        if (sqlite3_bind_double(stmt, index, value) != SQLITE_OK)
        {
            TraceLog(LOG_ERROR, "SQLiteQuery: Failed to bind double at index %d: %s", index, sqlite3_errmsg(db));
            valid = false;
        }
        return *this;
    }

    SQLiteQuery &bind(int index, const char *value)
    {
        if (!stmt || !valid)
            return *this;

        if (sqlite3_bind_text(stmt, index, value, -1, SQLITE_TRANSIENT) != SQLITE_OK)
        {
            TraceLog(LOG_ERROR, "SQLiteQuery: Failed to bind text at index %d: %s", index, sqlite3_errmsg(db));
            valid = false;
        }
        return *this;
    }

    SQLiteQuery &bind(int index, const std::string &value)
    {
        return bind(index, value.c_str());
    }

    inline void bind(int &param)
    {
        if (sqlite3_bind_int(stmt, 1, param) != SQLITE_OK)
        {
            valid = false;
        }
    }

    // cast to sqlite3_stmt* for convenience
    operator sqlite3_stmt *() const { return stmt; }
};
