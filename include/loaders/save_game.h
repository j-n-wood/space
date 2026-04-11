#pragma once

class System;
class Game;
class Location;
class ResourceFacility;
class Orbital;
class Stores;
class Factory;
class Loader;

class SaveGame
{
    Loader *loader;

    int initialiseSaveFile();
    int saveSystem(System *system);
    int saveGame(Game *game);
    int saveLocation(Location *location);
    int saveBase(ResourceFacility *rf);
    int saveOrbital(Orbital *orbital);
    int saveStores(Stores *stores);

public:
    SaveGame();
    ~SaveGame();

    int save(const char *filename);
};