#include <stdlib.h>

#include "loaders/loader.h"
#include "raylib.h"
#include "state/game.h"

Loader::Loader(const char *dbPath)
{
    if (sqlite3_open(dbPath, &db) != SQLITE_OK)
    {
        // sqlite3_open cleans up on failure, don't close
        db = nullptr;
        TraceLog(LOG_ERROR, "Failed to open database: %s", sqlite3_errmsg(db));
    }

    game = &Game::getInstance();
}

Loader::~Loader()
{
    if (db)
    {
        sqlite3_close(db);
        db = nullptr;
    }
}