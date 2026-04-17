#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../include/loaders/loader.h"
#include "../include/loaders/load_system.h"
#include "../include/loaders/save_game.h"
#include "../include/state/game.h"
#include "../include/state/system.h"
#include "../include/state/location.h"
#include "../include/state/facility.h"
#include "../include/state/resourceFacility.h"
#include "../include/state/orbital.h"
#include "../include/state/item.h"
#include "../include/state/resources.h"
#include <sqlite3.h>
#include <cstdio>
#include <cstring>

#define CHECK_STREQ(a, b) CHECK(std::strcmp((a), (b)) == 0)

static const char *DB_PATH = "./resources/initial.db";
static const char *SAVE_PATH = "./test_save.db";

TEST_CASE("Loader with in-memory database")
{
    Loader loader(":memory:");
    CHECK(loader.isValid());
}

TEST_CASE("Loader with invalid path")
{
    // sqlite3_open creates the file if it doesn't exist,
    // so an invalid path is one where the directory doesn't exist
    Loader loader("/nonexistent_dir/nonexistent.db");
    CHECK_FALSE(loader.isValid());
}

TEST_CASE("loadSystem populates system from database")
{
    Loader loader(DB_PATH);
    REQUIRE(loader.isValid());

    SystemPtr system = createSystem(false);
    REQUIRE(system);
    REQUIRE(loadSystem(&loader, 1, system.get()));

    SUBCASE("loads correct number of bodies")
    {
        CHECK(system->numPlanets == 12);
    }

    SUBCASE("creates locations for each body")
    {
        CHECK(system->locations.size() == 12);
    }

    SUBCASE("first location is the star Sol")
    {
        REQUIRE(system->locations.size() > 0);
        CHECK_STREQ(system->locations[0]->name, "Sol");
        CHECK(system->locations[0]->type == LOCATION_TYPE_STAR);
    }

    SUBCASE("planets are loaded in order")
    {
        REQUIRE(system->locations.size() >= 9);
        CHECK_STREQ(system->locations[1]->name, "Mercury");
        CHECK_STREQ(system->locations[2]->name, "Venus");
        CHECK_STREQ(system->locations[3]->name, "Earth");
        CHECK_STREQ(system->locations[4]->name, "Mars");
        CHECK_STREQ(system->locations[5]->name, "Jupiter");
        CHECK_STREQ(system->locations[6]->name, "Saturn");
        CHECK_STREQ(system->locations[7]->name, "Uranus");
        CHECK_STREQ(system->locations[8]->name, "Neptune");
    }

    SUBCASE("planets have correct location type")
    {
        REQUIRE(system->locations.size() >= 9);
        for (int i = 1; i <= 8; i++)
        {
            CHECK(system->locations[i]->type == LOCATION_TYPE_PLANET);
        }
    }

    SUBCASE("moons are loaded and linked to parent")
    {
        REQUIRE(system->locations.size() >= 12);
        // Luna (id=10) orbits Earth (id=4)
        CHECK_STREQ(system->locations[9]->name, "Luna");
        CHECK(system->locations[9]->type == LOCATION_TYPE_MOON);
        CHECK(system->locations[9]->primary_id == 4); // Earth's body id

        // Phobos and Deimos orbit Mars (id=5)
        CHECK_STREQ(system->locations[10]->name, "Phobos");
        CHECK(system->locations[10]->primary_id == 5);
        CHECK_STREQ(system->locations[11]->name, "Deimos");
        CHECK(system->locations[11]->primary_id == 5);
    }

    SUBCASE("parent-child relationships are built")
    {
        REQUIRE(system->locations.size() >= 12);
        // Earth should have Luna as a child
        Location *earth = system->locations[3].get();
        CHECK(earth->children.size() == 1);
        if (!earth->children.empty())
        {
            CHECK_STREQ(earth->children[0]->name, "Luna");
        }

        // Mars should have Phobos and Deimos
        Location *mars = system->locations[4].get();
        CHECK(mars->children.size() == 2);
    }

    SUBCASE("orbital data arrays are populated")
    {
        CHECK(system->planetDistances.size() == 12);
        CHECK(system->planetSizes.size() == 12);
        CHECK(system->planetColors.size() == 12);
        CHECK(system->planetVelocities.size() == 12);
        CHECK(system->planetPositions.size() == 12);
        CHECK(system->planetPrimaryIndexes.size() == 12);
    }

    SUBCASE("star has primary index -1")
    {
        CHECK(system->planetPrimaryIndexes[0] == -1);
    }
}

TEST_CASE("Game::initialise loads full game state")
{
    Game game;
    Loader loader(DB_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(game.initialise(&loader));

    SUBCASE("loads systems")
    {
        auto &systems = game.allSystems();
        REQUIRE(systems.size() == 1);
        CHECK_STREQ(systems[0]->name, "Sol");
        CHECK(systems[0]->id == 1);
    }

    SUBCASE("loads game time")
    {
        CHECK(game.game_time == 0.0f);
    }

    SUBCASE("loads item definitions")
    {
        REQUIRE(game.items.size() == 1);

        auto &derrick = game.items[0];
        CHECK_STREQ(derrick.name, "Derrick");
        CHECK(derrick.id == 0);
        CHECK(derrick.tech_level == 1);
        CHECK(derrick.production_time == 8);
    }

    SUBCASE("loads item build requirements")
    {
        REQUIRE(game.items.size() == 1);
        auto &reqs = game.items[0].requirements;
        // Derrick requires: Iron(1)=3, Titanium(2)=4, Carbon(4)=1
        CHECK(reqs.size() == 3);
    }

    SUBCASE("no facilities in initial database")
    {
        // initial.db has no facilities — they are created at game start
        CHECK(game.allBases().size() == 0);
        CHECK(game.allOrbitals().size() == 0);
    }
}

TEST_CASE("SQLiteQuery basic operations")
{
    Loader loader(":memory:");
    REQUIRE(loader.isValid());

    // Create a test table
    sqlite3_exec(loader.db, "CREATE TABLE test (id INTEGER, name TEXT, value REAL)", nullptr, nullptr, nullptr);
    sqlite3_exec(loader.db, "INSERT INTO test VALUES (1, 'alpha', 1.5)", nullptr, nullptr, nullptr);
    sqlite3_exec(loader.db, "INSERT INTO test VALUES (2, 'beta', 2.7)", nullptr, nullptr, nullptr);

    SUBCASE("iterates rows with next()")
    {
        SQLiteQuery q(&loader, "SELECT id, name FROM test ORDER BY id");
        REQUIRE(q.next());
        CHECK(sqlite3_column_int(q, 0) == 1);
        CHECK(std::strcmp((const char *)sqlite3_column_text(q, 1), "alpha") == 0);
        REQUIRE(q.next());
        CHECK(sqlite3_column_int(q, 0) == 2);
        CHECK_FALSE(q.next());
    }

    SUBCASE("bind int parameter")
    {
        SQLiteQuery q(&loader, "SELECT name FROM test WHERE id = ?");
        q.bind(1, 2);
        REQUIRE(q.next());
        CHECK(std::strcmp((const char *)sqlite3_column_text(q, 0), "beta") == 0);
        CHECK_FALSE(q.next());
    }

    SUBCASE("bind text parameter")
    {
        SQLiteQuery q(&loader, "SELECT id FROM test WHERE name = ?");
        q.bind(1, "alpha");
        REQUIRE(q.next());
        CHECK(sqlite3_column_int(q, 0) == 1);
    }

    SUBCASE("bind double parameter")
    {
        SQLiteQuery q(&loader, "SELECT name FROM test WHERE value > ?");
        q.bind(1, 2.0);
        REQUIRE(q.next());
        CHECK(std::strcmp((const char *)sqlite3_column_text(q, 0), "beta") == 0);
    }

    SUBCASE("step executes non-SELECT statements")
    {
        SQLiteQuery q(&loader, "INSERT INTO test VALUES (3, 'gamma', 3.0)");
        CHECK(q.step());

        SQLiteQuery verify(&loader, "SELECT count(*) FROM test");
        REQUIRE(verify.next());
        CHECK(sqlite3_column_int(verify, 0) == 3);
    }

    SUBCASE("chained binds")
    {
        SQLiteQuery q(&loader, "SELECT name FROM test WHERE id = ? AND value > ?");
        q.bind(1, 2).bind(2, 2.0);
        REQUIRE(q.next());
        CHECK(std::strcmp((const char *)sqlite3_column_text(q, 0), "beta") == 0);
    }

    SUBCASE("invalid SQL sets valid to false")
    {
        SQLiteQuery q(&loader, "SELECT * FROM nonexistent_table");
        CHECK_FALSE(q.next());
    }
}

// Helper: set up the Game singleton from initial.db, add facilities and stores,
// then return the raw pointer. The singleton owns the lifetime.
static Game *createTestGame()
{
    Game *game = Game::createCurrent();
    Loader loader(DB_PATH);
    if (!loader.isValid() || !game->initialise(&loader))
        return nullptr;

    // Add facilities like main.cpp does at startup
    System *system = game->allSystems()[0].get();
    Location *earth = system->locations[3].get(); // Earth

    ResourceFacility *rf = game->createResourceFacility(earth);
    rf->num_derricks = 3;
    rf->stores.resources[ResourceType::Iron] = 50;
    rf->stores.resources[ResourceType::Titanium] = 25;

    Orbital *orb = game->createOrbital(earth);
    orb->stores.resources[ResourceType::Carbon] = 10;

    game->game_time = 42.5f;

    return game;
}

// Helper: clean up the save file if it exists
static void removeSaveFile()
{
    remove(SAVE_PATH);
}

TEST_CASE("SaveGame produces a loadable database")
{
    Game *game = createTestGame();
    REQUIRE(game != nullptr);

    // Save
    SaveGame saver;
    REQUIRE(saver.save(SAVE_PATH) == 0);

    // Load into a fresh Game
    Game loaded;
    Loader loader(SAVE_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(loaded.initialise(&loader));

    SUBCASE("round-trips game time")
    {
        CHECK(loaded.game_time == doctest::Approx(42.5f));
    }

    SUBCASE("round-trips system")
    {
        auto &systems = loaded.allSystems();
        REQUIRE(systems.size() == 1);
        CHECK_STREQ(systems[0]->name, "Sol");
        CHECK(systems[0]->id == 1);
    }

    SUBCASE("round-trips all locations")
    {
        auto &sys = loaded.allSystems()[0];
        CHECK(sys->locations.size() == 12);

        // spot-check a few
        CHECK_STREQ(sys->locations[0]->name, "Sol");
        CHECK(sys->locations[0]->type == LOCATION_TYPE_STAR);
        CHECK_STREQ(sys->locations[3]->name, "Earth");
        CHECK(sys->locations[3]->type == LOCATION_TYPE_PLANET);
        CHECK_STREQ(sys->locations[9]->name, "Luna");
        CHECK(sys->locations[9]->type == LOCATION_TYPE_MOON);
    }

    SUBCASE("round-trips parent-child relationships")
    {
        auto &sys = loaded.allSystems()[0];
        REQUIRE(sys->locations.size() >= 12);

        Location *earth = sys->locations[3].get();
        CHECK(earth->children.size() == 1);

        Location *mars = sys->locations[4].get();
        CHECK(mars->children.size() == 2);
    }

    SUBCASE("round-trips resource facility")
    {
        auto &bases = loaded.allBases();
        REQUIRE(bases.size() == 1);

        auto &rf = bases[0];
        CHECK(rf->location != nullptr);
        CHECK_STREQ(rf->location->name, "Earth");
        CHECK(rf->num_derricks == 3);
    }

    SUBCASE("round-trips orbital")
    {
        auto &orbitals = loaded.allOrbitals();
        REQUIRE(orbitals.size() == 1);
        CHECK(orbitals[0]->location != nullptr);
        CHECK_STREQ(orbitals[0]->location->name, "Earth");
    }

    SUBCASE("round-trips facility stores")
    {
        auto &bases = loaded.allBases();
        REQUIRE(bases.size() == 1);
        CHECK(bases[0]->stores.resources[ResourceType::Iron] == 50);
        CHECK(bases[0]->stores.resources[ResourceType::Titanium] == 25);

        auto &orbitals = loaded.allOrbitals();
        REQUIRE(orbitals.size() == 1);
        CHECK(orbitals[0]->stores.resources[ResourceType::Carbon] == 10);
    }

    SUBCASE("round-trips item definitions")
    {
        REQUIRE(loaded.items.size() == 1);
        auto &item = loaded.items[0];
        CHECK_STREQ(item.name, "Derrick");
        CHECK(item.tech_level == 1);
        CHECK(item.production_time == 8);
    }

    SUBCASE("round-trips item build requirements")
    {
        REQUIRE(loaded.items.size() == 1);
        auto &reqs = loaded.items[0].requirements;
        REQUIRE(reqs.size() == 3);

        // Verify specific requirements (Iron=3, Titanium=4, Carbon=1)
        bool found_iron = false, found_titanium = false, found_carbon = false;
        for (auto &req : reqs)
        {
            if (req.resource == ResourceType::Iron && req.amount == 3)
                found_iron = true;
            if (req.resource == ResourceType::Titanium && req.amount == 4)
                found_titanium = true;
            if (req.resource == ResourceType::Carbon && req.amount == 1)
                found_carbon = true;
        }
        CHECK(found_iron);
        CHECK(found_titanium);
        CHECK(found_carbon);
    }

    SUBCASE("zero-amount stores are not saved")
    {
        // Verify that the save file only contains non-zero store entries
        Loader verifyLoader(SAVE_PATH);
        REQUIRE(verifyLoader.isValid());
        SQLiteQuery q(&verifyLoader, "SELECT count(*) FROM stores WHERE amount = 0");
        REQUIRE(q.next());
        CHECK(sqlite3_column_int(q, 0) == 0);
    }

    removeSaveFile();
}

TEST_CASE("SaveGame overwrites existing file")
{
    Game *game = createTestGame();
    REQUIRE(game != nullptr);

    // Save twice to the same path
    SaveGame saver1;
    REQUIRE(saver1.save(SAVE_PATH) == 0);
    SaveGame saver2;
    REQUIRE(saver2.save(SAVE_PATH) == 0);

    // Load and verify no duplicate data
    Game loaded;
    Loader loader(SAVE_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(loaded.initialise(&loader));

    CHECK(loaded.allSystems().size() == 1);
    CHECK(loaded.allBases().size() == 1);
    CHECK(loaded.allOrbitals().size() == 1);

    removeSaveFile();
}
