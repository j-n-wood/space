#pragma once

// handle wrapping SQLite handles and queries

#include "sqlite3.h"

extern "C"
{
#include "raylib.h" // logger
}

class Game;

class Loader
{
    Game *game;

public:
    explicit Loader(const char *dbPath);
    ~Loader();

    bool isValid() const { return db != nullptr; }

    sqlite3 *db;

    bool loadSystems();
};

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
