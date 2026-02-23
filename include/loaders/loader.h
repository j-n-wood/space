#pragma once

// handle wrapping SQLite handles and queries

#include "sqlite3.h"

typedef struct {
    sqlite3* db;
} Loader;

Loader* createLoader(const char* dbPath);
void destroyLoader(Loader* loader);