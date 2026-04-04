#pragma once

// handle wrapping SQLite handles and queries

#include "sqlite3.h"

class Loader {
public:
    explicit Loader(const char* dbPath);
    ~Loader();

    bool isValid() const { return db != nullptr; }

    sqlite3* db;
};