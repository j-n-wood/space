#pragma once

#include <cstdlib>

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
class Craft;

/// Saves the current game state into a SQLite database file.
/// Uses Loader and SQLiteQuery helpers for persistence of systems, locations, facilities, and stores.
class SaveGame
{
    Loader *loader;

    /// Create or verify the SQLite schema for the save file.
    int initialiseSaveFile();

    /// Persist one system and its bodies + facilities.
    int saveSystem(System *system);

    /// Persist top-level game metadata.
    int saveGame(Game *game);

    /// Persist one body within a system.
    int saveLocation(SQLiteQuery &bodyQuery, System *system, size_t locationIndex, sqlite3_int64 systemId);

    /// Persist one base facility.
    int saveBase(ResourceFacility *rf, int facilityId);

    /// Persist one orbital facility.
    int saveOrbital(Orbital *orbital, int facilityId);

    /// Persist store state belonging to a facility.
    int saveStores(Stores *stores, int facilityId);

    /// Persist item definitions
    int saveItems(Game *game);

    /// Persist research topics
    int saveResearchTopics(Game *game);

    /// Persist craft state (shuttles and IOS)
    int saveCraft(Game *game);
    int saveCraft(Craft *craft, int craftId);

    /// Persist craft destinations (for shuttles and IOS)
    int saveCraftDestinations(Craft *craft, int craftId);

    /// Persist autopilot against a craft
    int saveAutopilot(Craft *craft, int craftId);

public:
    SaveGame();
    ~SaveGame();

    /// Save full game state to the named SQLite file.
    int save(const char *filename);
};