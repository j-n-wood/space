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
#include "../include/state/autopilot.h"
#include "../include/state/craft.h"
#include "../include/state/shuttle.h"
#include "../include/state/ios.h"
#include "../include/state/waypoint.h"
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
    // loadSystem creates Locations via game->createLocation, and Orbital
    // construction touches Game::getCurrent()->createFactory, so we need a
    // singleton Game and a Loader whose game pointer is set.
    Game *game = Game::createCurrent();
    Loader loader(DB_PATH);
    REQUIRE(loader.isValid());
    loader.setGame(game);

    System *system = game->createSystem(1, "Sol");
    REQUIRE(system != nullptr);
    REQUIRE(loader.loadSystem(1, system));

    SUBCASE("loads bodies for system 1")
    {
        CHECK(system->numPlanets > 0);
        CHECK(system->locations.size() == static_cast<size_t>(system->numPlanets));
    }

    SUBCASE("first location is the star Sol")
    {
        REQUIRE(system->locations.size() > 0);
        CHECK_STREQ(system->locations[0]->name, "Sol");
        CHECK(system->locations[0]->type == LOCATION_TYPE_STAR);
    }

    SUBCASE("orbital data arrays match numPlanets")
    {
        const auto n = static_cast<size_t>(system->numPlanets);
        CHECK(system->planetDistances.size() == n);
        CHECK(system->planetSizes.size() == n);
        CHECK(system->planetColors.size() == n);
        CHECK(system->planetVelocities.size() == n);
        CHECK(system->planetPositions.size() == n);
        CHECK(system->planetPrimaryIndexes.size() == n);
    }

    SUBCASE("star has primary index -1")
    {
        REQUIRE(system->planetPrimaryIndexes.size() > 0);
        CHECK(system->planetPrimaryIndexes[0] == -1);
    }

    SUBCASE("parent-child relationships are built for non-star bodies")
    {
        // Every non-star body's primary_id refers to a real location in the
        // system, and that parent has the body in its children.
        for (Location *loc : system->locations)
        {
            if (loc->type == LOCATION_TYPE_STAR)
                continue;
            REQUIRE(loc->primary_id != 0);
            Location *parent = nullptr;
            for (Location *cand : system->locations)
            {
                if (cand->id == loc->primary_id)
                {
                    parent = cand;
                    break;
                }
            }
            REQUIRE(parent != nullptr);
            bool found = false;
            for (Location *child : parent->children)
            {
                if (child == loc)
                {
                    found = true;
                    break;
                }
            }
            CHECK(found);
        }
    }
}

TEST_CASE("Game::initialise loads full game state")
{
    // Use the singleton: Orbital's constructor reaches Game::getCurrent() to
    // attach a factory, so loading facilities into a stack-allocated Game
    // would crash when initial.db contains any orbital.
    Game *game = Game::createCurrent();
    Loader loader(DB_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(game->initialise(&loader));

    SUBCASE("loads systems")
    {
        auto &systems = game->allSystems();
        REQUIRE(systems.size() >= 1);
        CHECK_STREQ(systems[0]->name, "Sol");
        CHECK(systems[0]->id == 1);
    }

    SUBCASE("loads game time")
    {
        CHECK(game->game_time == 0.0f);
    }

    SUBCASE("loads item definitions")
    {
        REQUIRE(game->items.size() >= 1);

        auto &derrick = game->items[0];
        CHECK_STREQ(derrick.name, "Derrick");
        CHECK(derrick.id == 0);
        CHECK(derrick.tech_level == 1);
        CHECK(derrick.production_time == 8);
    }

    SUBCASE("loads item build requirements")
    {
        REQUIRE(game->items.size() >= 1);
        auto &reqs = game->items[0].requirements;
        // Derrick requires: Iron(1)=3, Titanium(2)=4, Carbon(4)=1
        CHECK(reqs.size() == 3);
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

    // Find Earth by ID — initial.db ordering may not put Earth at index 3
    // any more (asteroid belts and additional bodies shift earlier slots).
    Location *earth = game->locationByID(4);
    if (!earth)
        return nullptr;

    // initial.db now ships with a ResourceFacility and an Orbital at Earth.
    // Reuse them if present so we don't end up with duplicates after load.
    ResourceFacility *rf = game->resourceFacilityAt(earth);
    if (!rf)
        rf = game->createResourceFacility(earth);
    rf->num_derricks = 3;
    rf->stores.resources[ResourceType::Iron] = 50;
    rf->stores.resources[ResourceType::Titanium] = 25;

    Orbital *orb = game->orbitalAt(earth);
    if (!orb)
        orb = game->createOrbital(earth);
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

    // Load using the singleton: Orbital construction reaches
    // Game::getCurrent() to attach a factory.
    Game &loaded = *Game::createCurrent();
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
        REQUIRE(systems.size() >= 1);
        CHECK_STREQ(systems[0]->name, "Sol");
        CHECK(systems[0]->id == 1);
    }

    SUBCASE("round-trips all locations")
    {
        auto &sys = loaded.allSystems()[0];
        CHECK(sys->locations.size() > 0);

        // spot-check the star — ordering of later bodies depends on the
        // current initial.db schema, so just verify Sol is index 0.
        CHECK_STREQ(sys->locations[0]->name, "Sol");
        CHECK(sys->locations[0]->type == LOCATION_TYPE_STAR);

        // Earth has a stable id (4) regardless of body ordering.
        Location *earth = loaded.locationByID(4);
        REQUIRE(earth != nullptr);
        CHECK_STREQ(earth->name, "Earth");
        CHECK(earth->type == LOCATION_TYPE_PLANET);
    }

    SUBCASE("round-trips parent-child relationships")
    {
        Location *earth = loaded.locationByID(4);
        REQUIRE(earth != nullptr);
        // Earth has Luna as a child.
        CHECK(earth->children.size() >= 1);
        bool foundLuna = false;
        for (Location *child : earth->children)
        {
            if (std::strcmp(child->name, "Luna") == 0)
            {
                foundLuna = true;
                break;
            }
        }
        CHECK(foundLuna);
    }

    SUBCASE("round-trips resource facility")
    {
        Location *earth = loaded.locationByID(4);
        REQUIRE(earth != nullptr);
        ResourceFacility *rf = loaded.resourceFacilityAt(earth);
        REQUIRE(rf != nullptr);
        CHECK(rf->num_derricks == 3);
    }

    SUBCASE("round-trips orbital")
    {
        Location *earth = loaded.locationByID(4);
        REQUIRE(earth != nullptr);
        Orbital *orb = loaded.orbitalAt(earth);
        REQUIRE(orb != nullptr);
        CHECK_STREQ(orb->location->name, "Earth");
    }

    SUBCASE("round-trips facility stores")
    {
        Location *earth = loaded.locationByID(4);
        REQUIRE(earth != nullptr);
        ResourceFacility *rf = loaded.resourceFacilityAt(earth);
        REQUIRE(rf != nullptr);
        CHECK(rf->stores.resources[ResourceType::Iron] == 50);
        CHECK(rf->stores.resources[ResourceType::Titanium] == 25);

        Orbital *orb = loaded.orbitalAt(earth);
        REQUIRE(orb != nullptr);
        CHECK(orb->stores.resources[ResourceType::Carbon] == 10);
    }

    SUBCASE("round-trips item definitions")
    {
        REQUIRE(loaded.items.size() >= 1);
        auto &item = loaded.items[0];
        CHECK_STREQ(item.name, "Derrick");
        CHECK(item.tech_level == 1);
        CHECK(item.production_time == 8);
    }

    SUBCASE("round-trips item build requirements")
    {
        REQUIRE(loaded.items.size() >= 1);
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

    const auto preSystems = game->allSystems().size();
    const auto preBases = game->allBases().size();
    const auto preOrbitals = game->allOrbitals().size();

    // Save twice to the same path
    SaveGame saver1;
    REQUIRE(saver1.save(SAVE_PATH) == 0);
    SaveGame saver2;
    REQUIRE(saver2.save(SAVE_PATH) == 0);

    // Load using the singleton so Orbital construction can reach
    // Game::getCurrent() — and verify no duplicate data.
    Game *loaded = Game::createCurrent();
    Loader loader(SAVE_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(loaded->initialise(&loader));

    CHECK(loaded->allSystems().size() == preSystems);
    CHECK(loaded->allBases().size() == preBases);
    CHECK(loaded->allOrbitals().size() == preOrbitals);

    removeSaveFile();
}

TEST_CASE("Autopilot::nextFlagged cycles through flagged resources")
{
    Autopilot ap;
    ap.flow[ResourceType::Iron] = RF_LOAD_AT_SOURCE;
    ap.flow[ResourceType::Copper] = RF_BALANCE;
    ap.flow[ResourceType::Carbon] = RF_LOAD_AT_DEST;

    // cursor 0 sees RF_LOAD_AT_SOURCE: Iron and Copper
    CHECK(ap.nextFlagged(RF_LOAD_AT_SOURCE, &ap.cursors[0]) == ResourceType::Iron);
    CHECK(ap.nextFlagged(RF_LOAD_AT_SOURCE, &ap.cursors[0]) == ResourceType::Copper);
    CHECK(ap.nextFlagged(RF_LOAD_AT_SOURCE, &ap.cursors[0]) == ResourceType::Iron);

    // cursor 1 sees RF_LOAD_AT_DEST: Carbon (4) then Copper (5), then wrap
    CHECK(ap.nextFlagged(RF_LOAD_AT_DEST, &ap.cursors[1]) == ResourceType::Carbon);
    CHECK(ap.nextFlagged(RF_LOAD_AT_DEST, &ap.cursors[1]) == ResourceType::Copper);
    CHECK(ap.nextFlagged(RF_LOAD_AT_DEST, &ap.cursors[1]) == ResourceType::Carbon);
}

TEST_CASE("Autopilot::nextFlagged returns -1 when nothing flagged")
{
    Autopilot ap;
    CHECK(ap.nextFlagged(RF_LOAD_AT_SOURCE, &ap.cursors[0]) == -1);
    CHECK(ap.nextFlagged(RF_LOAD_AT_DEST, &ap.cursors[1]) == -1);
}

TEST_CASE("SaveGame round-trips game-level counters")
{
    Game *game = createTestGame();
    REQUIRE(game != nullptr);

    game->ios_number = 42;
    game->scg_number = 99;

    SaveGame saver;
    REQUIRE(saver.save(SAVE_PATH) == 0);

    Game loaded;
    Loader loader(SAVE_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(loaded.initialise(&loader));

    CHECK(loaded.ios_number == 42);
    CHECK(loaded.scg_number == 99);

    removeSaveFile();
}

TEST_CASE("SaveGame round-trips body resource availability")
{
    Game *game = createTestGame();
    REQUIRE(game != nullptr);

    Location *earth = game->allSystems()[0]->locations[3];
    earth->resources.availability[ResourceType::Iron] = 7;
    earth->resources.availability[ResourceType::Carbon] = 3;
    earth->resources.availability[ResourceType::HedFuel] = 1; // index 16, boundary

    SaveGame saver;
    REQUIRE(saver.save(SAVE_PATH) == 0);

    Game loaded;
    Loader loader(SAVE_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(loaded.initialise(&loader));

    Location *loadedEarth = loaded.allSystems()[0]->locations[3];
    CHECK(loadedEarth->resources.availability[ResourceType::Iron] == 7);
    CHECK(loadedEarth->resources.availability[ResourceType::Carbon] == 3);
    CHECK(loadedEarth->resources.availability[ResourceType::HedFuel] == 1);

    removeSaveFile();
}

TEST_CASE("SaveGame round-trips research topics")
{
    Game *game = createTestGame();
    REQUIRE(game != nullptr);
    REQUIRE(game->researchTopics.size() > 0);

    auto &t0 = game->researchTopics[0];
    t0.progress = 12.5f;
    t0.available = true;
    t0.unlocksItems.clear();
    t0.unlocksItems.push_back(static_cast<ItemType>(0));
    t0.unlocksTopics.clear();
    if (game->researchTopics.size() > 1)
    {
        t0.unlocksTopics.push_back(1);
    }

    SaveGame saver;
    REQUIRE(saver.save(SAVE_PATH) == 0);

    Game loaded;
    Loader loader(SAVE_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(loaded.initialise(&loader));

    REQUIRE(loaded.researchTopics.size() == game->researchTopics.size());
    auto &l0 = loaded.researchTopics[0];

    SUBCASE("round-trips progress")
    {
        CHECK(l0.progress == doctest::Approx(12.5f));
    }
    SUBCASE("round-trips available flag")
    {
        CHECK(l0.available == true);
    }
    SUBCASE("round-trips unlocksItems")
    {
        REQUIRE(l0.unlocksItems.size() == 1);
        CHECK(l0.unlocksItems[0] == static_cast<ItemType>(0));
    }
    SUBCASE("round-trips unlocksTopics")
    {
        if (game->researchTopics.size() > 1)
        {
            REQUIRE(l0.unlocksTopics.size() == 1);
            CHECK(l0.unlocksTopics[0] == 1);
        }
    }

    removeSaveFile();
}

TEST_CASE("SaveGame round-trips craft, pods, destinations, and autopilot")
{
    Game *game = createTestGame();
    REQUIRE(game != nullptr);

    Location *earth = game->allSystems()[0]->locations[3];
    REQUIRE(earth != nullptr);
    Orbital *orb = game->orbitalAt(earth);
    REQUIRE(orb != nullptr);

    // --- Shuttle: docked, drive fitted, pods loaded, autopilot AS_ON, varied flow + cursors
    Shuttle *shuttle = game->createShuttle(orb);
    REQUIRE(shuttle != nullptr);

    std::snprintf(shuttle->name, sizeof shuttle->name, "Discovery");
    shuttle->state = CS_ORBIT_DOCKED;
    shuttle->state_timer = 0.0f;
    shuttle->total_state_timer = 0.0f;
    shuttle->fuel = 100;
    shuttle->max_pods = 2;
    shuttle->drive = true;
    shuttle->destination_index = 1;

    shuttle->pods[0].type = PT_SUPPLY;
    shuttle->pods[0].contentType = ResourceType::Iron;
    shuttle->pods[0].amount = 75;
    shuttle->pods[1].type = PT_TOOL;
    shuttle->pods[1].contentType = 0; // Derrick
    shuttle->pods[1].amount = 2;

    shuttle->destinations[0] = Endpoint(earth, SLOC_SURFACE, true);
    shuttle->destinations[1] = Endpoint(earth, SLOC_ORBIT, false);

    shuttle->autopilot->state = AS_ON;
    for (int i = 0; i < ResourceType::Count; ++i)
    {
        shuttle->autopilot->flow[i] = 0;
    }
    shuttle->autopilot->flow[ResourceType::Iron] = RF_LOAD_AT_SOURCE;        // first non-trivial index
    shuttle->autopilot->flow[ResourceType::Copper] = RF_BALANCE;
    shuttle->autopilot->flow[ResourceType::HedFuel] = RF_LOAD_AT_DEST;       // boundary: ResourceType::Count - 1 (= 16)
    shuttle->autopilot->cursors[0] = 5;
    shuttle->autopilot->cursors[1] = 12;

    // --- IOS: terminal state, autopilot AS_COMPLETE
    IOS *ios = game->createIOS(orb);
    REQUIRE(ios != nullptr);
    std::snprintf(ios->name, sizeof ios->name, "IOS-Test");
    ios->state = CS_ORBIT_DOCKED;
    ios->state_timer = 1.5f;
    ios->total_state_timer = 3.0f;
    ios->fuel = 250;
    ios->drive = true;
    ios->autopilot->state = AS_COMPLETE;

    SaveGame saver;
    REQUIRE(saver.save(SAVE_PATH) == 0);

    Game loaded;
    Loader loader(SAVE_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(loaded.initialise(&loader));

    const auto &lShuttles = loaded.allShuttles();
    REQUIRE(lShuttles.size() == 1);
    Shuttle *ls = lShuttles[0];

    const auto &lIOS = loaded.allIOS();
    REQUIRE(lIOS.size() == 1);
    IOS *li = lIOS[0].get();

    SUBCASE("round-trips shuttle fields")
    {
        CHECK_STREQ(ls->name, "Discovery");
        CHECK(ls->type == CT_SHUTTLE);
        CHECK(ls->state == CS_ORBIT_DOCKED);
        CHECK(ls->state_timer == doctest::Approx(0.0f));
        CHECK(ls->total_state_timer == doctest::Approx(0.0f));
        CHECK(ls->fuel == 100);
        CHECK(ls->max_pods == 2);
        CHECK(ls->drive == true);
        CHECK(ls->destination_index == 1);
        REQUIRE(ls->location != nullptr);
        CHECK_STREQ(ls->location->name, "Earth");
    }

    SUBCASE("round-trips IOS fields")
    {
        CHECK_STREQ(li->name, "IOS-Test");
        CHECK(li->type == CT_IOS);
        CHECK(li->state == CS_ORBIT_DOCKED);
        CHECK(li->state_timer == doctest::Approx(1.5f));
        CHECK(li->total_state_timer == doctest::Approx(3.0f));
        CHECK(li->fuel == 250);
        CHECK(li->drive == true);
    }

    SUBCASE("round-trips pods")
    {
        CHECK(ls->pods[0].type == PT_SUPPLY);
        CHECK(ls->pods[0].contentType == ResourceType::Iron);
        CHECK(ls->pods[0].amount == 75);
        CHECK(ls->pods[1].type == PT_TOOL);
        CHECK(ls->pods[1].contentType == 0);
        CHECK(ls->pods[1].amount == 2);
    }

    SUBCASE("round-trips destinations")
    {
        REQUIRE(ls->destinations[0].location != nullptr);
        CHECK_STREQ(ls->destinations[0].location->name, "Earth");
        CHECK(ls->destinations[0].sublocation == SLOC_SURFACE);
        CHECK(ls->destinations[0].docked == true);

        REQUIRE(ls->destinations[1].location != nullptr);
        CHECK_STREQ(ls->destinations[1].location->name, "Earth");
        CHECK(ls->destinations[1].sublocation == SLOC_ORBIT);
        CHECK(ls->destinations[1].docked == false);
    }

    SUBCASE("round-trips autopilot state (AS_ON for shuttle)")
    {
        // Regression guard for the `> 0` bug fixed in a6abd19.
        CHECK(ls->autopilot->state == AS_ON);
    }

    SUBCASE("round-trips autopilot state (AS_COMPLETE for IOS)")
    {
        // Regression guard: any value > 1 was collapsed to AS_OFF prior to a6abd19.
        CHECK(li->autopilot->state == AS_COMPLETE);
    }

    SUBCASE("round-trips autopilot flow at low and high indices")
    {
        CHECK(ls->autopilot->flow[ResourceType::Iron] == RF_LOAD_AT_SOURCE);
        CHECK(ls->autopilot->flow[ResourceType::Copper] == RF_BALANCE);
        // Boundary: last legal index (ResourceType::Count - 1).
        CHECK(ls->autopilot->flow[ResourceType::HedFuel] == RF_LOAD_AT_DEST);
        // Untouched entry should remain zero.
        CHECK(ls->autopilot->flow[ResourceType::Aluminium] == 0);
    }

    SUBCASE("round-trips autopilot cursors with non-zero values")
    {
        // Regression guard for the column-type bug fixed in a6abd19.
        CHECK(ls->autopilot->cursors[0] == 5);
        CHECK(ls->autopilot->cursors[1] == 12);
    }

    removeSaveFile();
}

TEST_CASE("SaveGame round-trips autopilot state across all values")
{
    // Exercises every AutopilotState value independently to guard the recently
    // fixed `> 0` bug across the full enum range.

    auto roundTripWithAutopilot = [](AutopilotState s)
    {
        Game *game = createTestGame();
        REQUIRE(game != nullptr);

        Location *earth = game->allSystems()[0]->locations[3];
        REQUIRE(earth != nullptr);
        Orbital *orb = game->orbitalAt(earth);
        REQUIRE(orb != nullptr);

        Shuttle *shuttle = game->createShuttle(orb);
        REQUIRE(shuttle != nullptr);
        shuttle->autopilot->state = s;

        SaveGame saver;
        REQUIRE(saver.save(SAVE_PATH) == 0);

        Game loaded;
        Loader loader(SAVE_PATH);
        REQUIRE(loader.isValid());
        REQUIRE(loaded.initialise(&loader));

        REQUIRE(loaded.allShuttles().size() == 1);
        CHECK(loaded.allShuttles()[0]->autopilot->state == s);

        removeSaveFile();
    };

    SUBCASE("AS_DISABLED") { roundTripWithAutopilot(AS_DISABLED); }
    SUBCASE("AS_OFF") { roundTripWithAutopilot(AS_OFF); }
    SUBCASE("AS_ON") { roundTripWithAutopilot(AS_ON); }
    SUBCASE("AS_COMPLETE") { roundTripWithAutopilot(AS_COMPLETE); }
}

TEST_CASE("Autopilot::nextFlagged with predicate skips rejected resources")
{
    Autopilot ap;
    ap.flow[ResourceType::Iron] = RF_LOAD_AT_SOURCE;
    ap.flow[ResourceType::Copper] = RF_LOAD_AT_SOURCE;
    ap.flow[ResourceType::Carbon] = RF_LOAD_AT_SOURCE;

    // Accept only Copper — Iron and Carbon should be skipped.
    auto onlyCopper = [](int r)
    { return r == ResourceType::Copper; };
    CHECK(ap.nextFlagged(RF_LOAD_AT_SOURCE, &ap.cursors[0], onlyCopper) == ResourceType::Copper);
    CHECK(ap.nextFlagged(RF_LOAD_AT_SOURCE, &ap.cursors[0], onlyCopper) == ResourceType::Copper);

    // Predicate rejects everything → -1.
    auto none = [](int)
    { return false; };
    CHECK(ap.nextFlagged(RF_LOAD_AT_SOURCE, &ap.cursors[0], none) == -1);
}
