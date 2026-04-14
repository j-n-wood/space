#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../include/loaders/loader.h"
#include "../include/loaders/load_system.h"
#include "../include/state/game.h"
#include "../include/state/system.h"
#include "../include/state/location.h"
#include "../include/state/facility.h"
#include "../include/state/resourceFacility.h"
#include "../include/state/orbital.h"
#include "../include/state/item.h"
#include <sqlite3.h>

static const char *DB_PATH = "./resources/initial.db";

TEST_CASE("Loader with in-memory database") {
    Loader loader(":memory:");
    CHECK(loader.isValid());
}

TEST_CASE("Loader with invalid path") {
    // sqlite3_open creates the file if it doesn't exist,
    // so an invalid path is one where the directory doesn't exist
    Loader loader("/nonexistent_dir/nonexistent.db");
    CHECK_FALSE(loader.isValid());
}

TEST_CASE("loadSystem populates system from database") {
    Loader loader(DB_PATH);
    REQUIRE(loader.isValid());

    SystemPtr system = createSystem(false);
    REQUIRE(system);
    REQUIRE(loadSystem(&loader, 1, system.get()));

    SUBCASE("loads correct number of bodies") {
        CHECK(system->numPlanets == 12);
    }

    SUBCASE("creates locations for each body") {
        CHECK(system->locations.size() == 12);
    }

    SUBCASE("first location is the star Sol") {
        REQUIRE(system->locations.size() > 0);
        CHECK(system->locations[0]->name == "Sol");
        CHECK(system->locations[0]->type == LOCATION_TYPE_STAR);
    }

    SUBCASE("planets are loaded in order") {
        REQUIRE(system->locations.size() >= 9);
        CHECK(system->locations[1]->name == "Mercury");
        CHECK(system->locations[2]->name == "Venus");
        CHECK(system->locations[3]->name == "Earth");
        CHECK(system->locations[4]->name == "Mars");
        CHECK(system->locations[5]->name == "Jupiter");
        CHECK(system->locations[6]->name == "Saturn");
        CHECK(system->locations[7]->name == "Uranus");
        CHECK(system->locations[8]->name == "Neptune");
    }

    SUBCASE("planets have correct location type") {
        REQUIRE(system->locations.size() >= 9);
        for (int i = 1; i <= 8; i++) {
            CHECK(system->locations[i]->type == LOCATION_TYPE_PLANET);
        }
    }

    SUBCASE("moons are loaded and linked to parent") {
        REQUIRE(system->locations.size() >= 12);
        // Luna (id=10) orbits Earth (id=4)
        CHECK(system->locations[9]->name == "Luna");
        CHECK(system->locations[9]->type == LOCATION_TYPE_MOON);
        CHECK(system->locations[9]->primary_id == 4); // Earth's body id

        // Phobos and Deimos orbit Mars (id=5)
        CHECK(system->locations[10]->name == "Phobos");
        CHECK(system->locations[10]->primary_id == 5);
        CHECK(system->locations[11]->name == "Deimos");
        CHECK(system->locations[11]->primary_id == 5);
    }

    SUBCASE("parent-child relationships are built") {
        REQUIRE(system->locations.size() >= 12);
        // Earth should have Luna as a child
        Location *earth = system->locations[3].get();
        CHECK(earth->children.size() == 1);
        if (!earth->children.empty()) {
            CHECK(earth->children[0]->name == "Luna");
        }

        // Mars should have Phobos and Deimos
        Location *mars = system->locations[4].get();
        CHECK(mars->children.size() == 2);
    }

    SUBCASE("orbital data arrays are populated") {
        CHECK(system->planetDistances.size() == 12);
        CHECK(system->planetSizes.size() == 12);
        CHECK(system->planetColors.size() == 12);
        CHECK(system->planetVelocities.size() == 12);
        CHECK(system->planetPositions.size() == 12);
        CHECK(system->planetPrimaryIndexes.size() == 12);
    }

    SUBCASE("star has primary index -1") {
        CHECK(system->planetPrimaryIndexes[0] == -1);
    }
}

TEST_CASE("Game::initialise loads full game state") {
    Game game;
    Loader loader(DB_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(game.initialise(&loader));

    SUBCASE("loads systems") {
        auto &systems = game.allSystems();
        REQUIRE(systems.size() == 1);
        CHECK(systems[0]->name == "Sol");
        CHECK(systems[0]->id == 1);
    }

    SUBCASE("sets current system") {
        CHECK(game.getCurrentSystem() != nullptr);
        CHECK(game.getCurrentSystem()->name == "Sol");
    }

    SUBCASE("loads game time") {
        CHECK(game.game_time == 0.0f);
    }

    SUBCASE("loads item definitions") {
        REQUIRE(game.items.size() == 1);

        auto &derrick = game.items[0];
        CHECK(derrick.name == "Derrick");
        CHECK(derrick.id == 0);
        CHECK(derrick.tech_level == 1);
        CHECK(derrick.production_time == 8);
    }

    SUBCASE("loads item build requirements") {
        REQUIRE(game.items.size() == 1);
        auto &reqs = game.items[0].requirements;
        // Derrick requires: Iron(1)=3, Titanium(2)=4, Carbon(4)=1
        CHECK(reqs.size() == 3);
    }

    SUBCASE("no facilities in initial database") {
        // initial.db has no facilities — they are created at game start
        CHECK(game.allBases().size() == 0);
        CHECK(game.allOrbitals().size() == 0);
    }
}

TEST_CASE("SQLiteQuery basic operations") {
    Loader loader(":memory:");
    REQUIRE(loader.isValid());

    // Create a test table
    sqlite3_exec(loader.db, "CREATE TABLE test (id INTEGER, name TEXT, value REAL)", nullptr, nullptr, nullptr);
    sqlite3_exec(loader.db, "INSERT INTO test VALUES (1, 'alpha', 1.5)", nullptr, nullptr, nullptr);
    sqlite3_exec(loader.db, "INSERT INTO test VALUES (2, 'beta', 2.7)", nullptr, nullptr, nullptr);

    SUBCASE("iterates rows with next()") {
        SQLiteQuery q(&loader, "SELECT id, name FROM test ORDER BY id");
        REQUIRE(q.next());
        CHECK(sqlite3_column_int(q, 0) == 1);
        CHECK(std::string((char *)sqlite3_column_text(q, 1)) == "alpha");
        REQUIRE(q.next());
        CHECK(sqlite3_column_int(q, 0) == 2);
        CHECK_FALSE(q.next());
    }

    SUBCASE("bind int parameter") {
        SQLiteQuery q(&loader, "SELECT name FROM test WHERE id = ?");
        q.bind(1, 2);
        REQUIRE(q.next());
        CHECK(std::string((char *)sqlite3_column_text(q, 0)) == "beta");
        CHECK_FALSE(q.next());
    }

    SUBCASE("bind text parameter") {
        SQLiteQuery q(&loader, "SELECT id FROM test WHERE name = ?");
        q.bind(1, "alpha");
        REQUIRE(q.next());
        CHECK(sqlite3_column_int(q, 0) == 1);
    }

    SUBCASE("bind double parameter") {
        SQLiteQuery q(&loader, "SELECT name FROM test WHERE value > ?");
        q.bind(1, 2.0);
        REQUIRE(q.next());
        CHECK(std::string((char *)sqlite3_column_text(q, 0)) == "beta");
    }

    SUBCASE("step executes non-SELECT statements") {
        SQLiteQuery q(&loader, "INSERT INTO test VALUES (3, 'gamma', 3.0)");
        CHECK(q.step());

        SQLiteQuery verify(&loader, "SELECT count(*) FROM test");
        REQUIRE(verify.next());
        CHECK(sqlite3_column_int(verify, 0) == 3);
    }

    SUBCASE("chained binds") {
        SQLiteQuery q(&loader, "SELECT name FROM test WHERE id = ? AND value > ?");
        q.bind(1, 2).bind(2, 2.0);
        REQUIRE(q.next());
        CHECK(std::string((char *)sqlite3_column_text(q, 0)) == "beta");
    }

    SUBCASE("invalid SQL sets valid to false") {
        SQLiteQuery q(&loader, "SELECT * FROM nonexistent_table");
        CHECK_FALSE(q.next());
    }
}
