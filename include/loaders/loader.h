#pragma once

// handle wrapping SQLite handles and queries

#include "sqlite3.h"

extern "C" {
    #include "raylib.h" // logger
}

class Loader {
public:
    explicit Loader(const char* dbPath);
    ~Loader();

    bool isValid() const { return db != nullptr; }

    sqlite3* db;
};

class SQLiteQuery {
public:
    sqlite3*    db;
    sqlite3_stmt* stmt;

    SQLiteQuery(Loader* loader, const char* sql) : db(loader->db), stmt(nullptr) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
            TraceLog(LOG_ERROR, "Failed to prepare query: %s", sqlite3_errmsg(db));
        }
    }

    ~SQLiteQuery() {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
    }

    bool next() {
        if (!stmt) return false;
        return sqlite3_step(stmt) == SQLITE_ROW;
    }
};
