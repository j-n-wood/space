#pragma once

#include "sqlite3.h"

class System;
class Game;
class Location;
class ResourceFacility;
class Orbital;
class Stores;
class Factory;
class Loader;
class SQLiteQuery;

class SaveGame
{
    Loader *loader;

    int initialiseSaveFile();
    int saveSystem(System *system);
    int saveGame(Game *game);
    int saveLocation(SQLiteQuery &bodyQuery, System *system, size_t locationIndex, sqlite3_int64 systemId);
    int saveBase(ResourceFacility *rf, int facilityId);
    int saveOrbital(Orbital *orbital, int facilityId);
    int saveStores(Stores *stores, int facilityId);

public:
    SaveGame();
    ~SaveGame();

    int save(const char *filename);
};