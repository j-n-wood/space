#include <stdlib.h>

#include "loaders/loader.h"
#include "raylib.h"

Loader* createLoader(const char* dbPath)
{
    Loader* loader = (Loader*)calloc(1, sizeof(Loader));
    if (sqlite3_open(dbPath, &loader->db) != SQLITE_OK)
    {
        // sqlite3_open cleans up on failure, don't close
        free(loader);
        return NULL;
    }
    return loader;
}

void destroyLoader(Loader* loader)
{
    if (loader == NULL) return;
    // Skip sqlite3_close - let OS cleanup handle it on process exit
    // This avoids issues with unfinalized statements or heap corruption
    free(loader);
}