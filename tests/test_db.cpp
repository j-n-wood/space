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
#include "../include/state/earth_city.h"
#include "../include/state/factory.h"
#include "../include/state/research_facility.h"
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

// Each system carries an interstellar-space body at locations[0] (id 0),
// so the star is not locations[0]. Look it up by type instead of assuming
// a fixed index.
static Location *findStar(System *system)
{
    for (Location *loc : system->locations)
    {
        if (loc->type == LOCATION_TYPE_STAR)
            return loc;
    }
    return nullptr;
}

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

    REQUIRE(loader.loadSystems());

    System *system = game->allSystems()[1].get();

    SUBCASE("loads bodies for system 1")
    {
        CHECK(system->bodyCount() > 0);
        CHECK(system->locations.size() == system->bodyCount());
    }

    SUBCASE("first location is the star Sol")
    {
        REQUIRE(system->locations.size() > 0);
        Location *star = findStar(system);
        REQUIRE(star != nullptr);
        CHECK_STREQ(star->name, "Sol");
        CHECK(star->type == LOCATION_TYPE_STAR);
    }

    SUBCASE("every location carries its own orbital elements")
    {
        // Orbital elements live on Location now, so there are no parallel arrays to
        // keep aligned. Every orbiting body must have a non-zero radius and a primary.
        for (Location *loc : system->locations)
        {
            if (loc->type == LOCATION_TYPE_PLANET || loc->type == LOCATION_TYPE_MOON)
            {
                CHECK(loc->orbital_radius > 0.0f);
                CHECK(loc->primary != nullptr);
            }
        }
    }

    SUBCASE("star and space have no primary")
    {
        Location *star = findStar(system);
        REQUIRE(star != nullptr);
        CHECK(star->primary == nullptr);
        REQUIRE(system->space != nullptr);
        CHECK(system->space->primary == nullptr);
    }

    SUBCASE("parent-child relationships are built for non-star bodies")
    {
        // Every non-star body's primary_id refers to a real location in the
        // system, and that parent has the body in its children.
        for (Location *loc : system->locations)
        {
            // Stars have no parent; interstellar-space bodies point at the
            // shared space body in system 0, which lives outside this system.
            if (loc->type == LOCATION_TYPE_STAR || loc->type == LOCATION_TYPE_SPACE)
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
        REQUIRE(systems.size() >= 2);
        // systems[0] is interstellar space (id 0); Sol is the first real system.
        CHECK_STREQ(systems[1]->name, "Sol");
        CHECK(systems[1]->id == 1);
    }

    SUBCASE("loads game time")
    {
        CHECK(game->game_time == 0.0);
    }

    SUBCASE("loads item definitions")
    {
        REQUIRE(game->items.size() >= 2);

        // items[0] is the "Empty" sentinel (id 0); Derrick is the first
        // real item.
        auto &derrick = game->items[1];
        CHECK_STREQ(derrick.name, "Derrick");
        CHECK(derrick.id == 1);
        CHECK(derrick.tech_level == 1);
        CHECK(derrick.production_time == 8);
    }

    SUBCASE("loads item build requirements")
    {
        REQUIRE(game->items.size() >= 2);
        auto &reqs = game->items[1].requirements;
        // Derrick requires: Iron(1)=20, Titanium(2)=50, Aluminium(3)=35,
        // Carbon(4)=10, Copper(5)=15
        CHECK(reqs.size() == 5);
    }
}

TEST_CASE("Game::initialise works on a stack-allocated Game (no singleton)")
{
    // initial.db ships with both an Orbital and an EarthCity at Earth. Both
    // constructors used to reach Game::getCurrent() during construction
    // (Orbital for createFactory, EarthCity for createResearchFacility),
    // which crashed when the singleton was unset. The owning Game methods
    // now wire those collaborators themselves, so any Game instance can
    // initialise from disk.
    {
        std::unique_ptr<Game> empty;
        Game::setCurrent(empty);
    }
    REQUIRE(Game::getCurrent() == nullptr);

    Game game;
    Loader loader(DB_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(game.initialise(&loader));

    Location *earth = game.locationByID(4);
    REQUIRE(earth != nullptr);

    Orbital *orb = game.orbitalAt(earth);
    REQUIRE(orb != nullptr);
    CHECK(orb->factory != nullptr); // wired by Game::createOrbital

    // Find the EarthCity in bases. LocationType is the discriminator now, so this
    // no longer needs RTTI.
    EarthCity *ec = nullptr;
    for (ResourceFacility *b : game.allBases())
    {
        if (b->type == LOCATION_TYPE_EARTH_CITY)
        {
            ec = static_cast<EarthCity *>(b);
            break;
        }
    }
    REQUIRE(ec != nullptr);
    CHECK(ec->factory != nullptr);           // wired by Game::createEarthCity
    CHECK(ec->research_facility != nullptr); // wired by Game::createEarthCity

    // Singleton is still null — the entire load path stayed on this Game.
    CHECK(Game::getCurrent() == nullptr);
}

TEST_CASE("Game::createOrbital works on a stack-allocated Game (no singleton)")
{
    // Regression guard: Orbital's constructor used to call
    // Game::getCurrent()->createFactory(this), which crashed if no Game
    // singleton was set. The factory creation now lives in
    // Game::createOrbital itself, so the orbital can be built by any Game
    // instance — including one allocated on the stack.
    //
    // Clear any prior singleton to prove createOrbital does not reach for it.
    {
        std::unique_ptr<Game> empty;
        Game::setCurrent(empty);
    }
    REQUIRE(Game::getCurrent() == nullptr);

    Game game;
    System *sys = game.createSystem(1, "TestSys");
    Location *body = game.createLocation(sys, 1, "TestBody", LOCATION_TYPE_PLANET);
    REQUIRE(body != nullptr);

    // Bodies come from the database with their regions already attached, so a
    // synthetic one has to be built the same way the loader builds it.
    Location *orbitRegion = game.createLocation(sys, 2, "TestBody Orbit", LOCATION_TYPE_ORBIT);
    REQUIRE(orbitRegion != nullptr);
    orbitRegion->primary = body;
    orbitRegion->primary_id = body->id;
    body->children.push_back(orbitRegion);

    Orbital *orb = game.createOrbital(body);
    REQUIRE(orb != nullptr);
    REQUIRE(body->orbit() == orbitRegion);
    CHECK(orb->primary == orbitRegion);
    CHECK(orb->body() == body);
    CHECK(orb->inOrbit());
    // The factory is what used to be wired via Game::getCurrent(); now wired
    // by Game::createOrbital itself.
    CHECK(orb->factory != nullptr);

    // Singleton remained empty — proves the construction path did not depend
    // on it.
    CHECK(Game::getCurrent() == nullptr);
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

    game->game_time = 42.5;

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
        CHECK(loaded.game_time == doctest::Approx(42.5));
    }

    SUBCASE("round-trips system")
    {
        auto &systems = loaded.allSystems();
        REQUIRE(systems.size() >= 2);
        // systems[0] is interstellar space (id 0); Sol is the first real system.
        CHECK_STREQ(systems[1]->name, "Sol");
        CHECK(systems[1]->id == 1);
    }

    SUBCASE("round-trips all locations")
    {
        auto &sys = loaded.allSystems()[1];
        CHECK(sys->locations.size() > 0);

        // spot-check the star — ordering of bodies depends on the current
        // initial.db schema, so look it up by type rather than by index.
        Location *star = findStar(sys.get());
        REQUIRE(star != nullptr);
        CHECK_STREQ(star->name, "Sol");
        CHECK(star->type == LOCATION_TYPE_STAR);

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
        // parented on Earth's orbit region now, so ask for the body
        CHECK_STREQ(orb->body()->name, "Earth");
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
        REQUIRE(loaded.items.size() >= 2);
        // items[0] is the "Empty" sentinel; Derrick is the first real item.
        auto &item = loaded.items[1];
        CHECK_STREQ(item.name, "Derrick");
        CHECK(item.tech_level == 1);
        CHECK(item.production_time == 8);
    }

    SUBCASE("round-trips item build requirements")
    {
        REQUIRE(loaded.items.size() >= 2);
        auto &reqs = loaded.items[1].requirements;
        REQUIRE(reqs.size() == 5);

        // Verify specific requirements (Iron=20, Titanium=50, Carbon=10)
        bool found_iron = false, found_titanium = false, found_carbon = false;
        for (auto &req : reqs)
        {
            if (req.resource == ResourceType::Iron && req.amount == 20)
                found_iron = true;
            if (req.resource == ResourceType::Titanium && req.amount == 50)
                found_titanium = true;
            if (req.resource == ResourceType::Carbon && req.amount == 10)
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

    Location *earth = game->locationByID(4);
    REQUIRE(earth != nullptr);
    earth->resources.availability[ResourceType::Iron] = 7;
    earth->resources.availability[ResourceType::Carbon] = 3;
    earth->resources.availability[ResourceType::HedFuel] = 1; // index 16, boundary

    SaveGame saver;
    REQUIRE(saver.save(SAVE_PATH) == 0);

    Game loaded;
    Loader loader(SAVE_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(loaded.initialise(&loader));

    Location *loadedEarth = loaded.locationByID(4);
    REQUIRE(loadedEarth != nullptr);
    CHECK(loadedEarth->resources.availability[ResourceType::Iron] == 7);
    CHECK(loadedEarth->resources.availability[ResourceType::Carbon] == 3);
    CHECK(loadedEarth->resources.availability[ResourceType::HedFuel] == 1);

    removeSaveFile();
}

TEST_CASE("Game::initialise loads factions from initial.db")
{
    Game *game = createTestGame();
    REQUIRE(game != nullptr);

    // initial.db ships with Terran (0) and Methanoid (1).
    REQUIRE(game->factions.size() >= 2);
    CHECK(game->factions[0].id == 0);
    CHECK_STREQ(game->factions[0].name, "Terran");
    CHECK(game->factions[1].id == 1);
    CHECK_STREQ(game->factions[1].name, "Methanoid");
}

TEST_CASE("SaveGame round-trips factions and facility faction ownership")
{
    Game *game = createTestGame();
    REQUIRE(game != nullptr);
    REQUIRE(game->factions.size() >= 2);

    // Flip a hostility flag so the boolean column is exercised too.
    game->factions[1].hostile = true;

    // Tag Earth's orbital with a non-default faction.
    Location *earth = game->locationByID(4);
    REQUIRE(earth != nullptr);
    Orbital *orb = game->orbitalAt(earth);
    REQUIRE(orb != nullptr);
    orb->faction_id = 1;

    SaveGame saver;
    REQUIRE(saver.save(SAVE_PATH) == 0);

    Game loaded;
    Loader loader(SAVE_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(loaded.initialise(&loader));

    SUBCASE("round-trips faction definitions and hostility")
    {
        REQUIRE(loaded.factions.size() >= 2);
        CHECK_STREQ(loaded.factions[0].name, "Terran");
        CHECK(loaded.factions[0].hostile == false);
        CHECK_STREQ(loaded.factions[1].name, "Methanoid");
        CHECK(loaded.factions[1].hostile == true);
    }

    SUBCASE("round-trips facility faction_id")
    {
        Location *loadedEarth = loaded.locationByID(4);
        REQUIRE(loadedEarth != nullptr);
        Orbital *lOrb = loaded.orbitalAt(loadedEarth);
        REQUIRE(lOrb != nullptr);
        CHECK(lOrb->faction_id == 1);
    }

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

TEST_CASE("SaveGame round-trips factory queue")
{
    Game *game = createTestGame();
    REQUIRE(game != nullptr);

    Location *earth = game->locationByID(4);
    REQUIRE(earth != nullptr);
    Orbital *orb = game->orbitalAt(earth);
    REQUIRE(orb != nullptr);
    REQUIRE(orb->factory != nullptr);
    REQUIRE(game->items.size() >= 3);

    // item id 0 is the "None"/"Empty" sentinel (not a queueable item), so
    // use the first two real items: Derrick (1) and S Chassis (2).

    // First item: started, mid-progress, repeating.
    QueueItem first(1, true);
    first.progress = 3;
    first.started = true;
    orb->factory->queue.push_back(first);

    // Second item: not yet started, non-repeating. Captures the
    // resources-not-yet-deducted state.
    QueueItem second(2, false);
    second.progress = 0;
    second.started = false;
    orb->factory->queue.push_back(second);

    SaveGame saver;
    REQUIRE(saver.save(SAVE_PATH) == 0);

    Game loaded;
    Loader loader(SAVE_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(loaded.initialise(&loader));

    Location *loadedEarth = loaded.locationByID(4);
    REQUIRE(loadedEarth != nullptr);
    Orbital *lOrb = loaded.orbitalAt(loadedEarth);
    REQUIRE(lOrb != nullptr);
    REQUIRE(lOrb->factory != nullptr);
    REQUIRE(lOrb->factory->queue.size() == 2);

    SUBCASE("preserves queue order")
    {
        CHECK(lOrb->factory->queue[0].item_id == 1);
        CHECK(lOrb->factory->queue[1].item_id == 2);
    }

    SUBCASE("preserves started/progress on in-flight item")
    {
        const QueueItem &qi = lOrb->factory->queue[0];
        CHECK(qi.progress == 3);
        CHECK(qi.started == true);
        CHECK(qi.repeat == true);
        CHECK(qi.build_time == first.build_time);
    }

    SUBCASE("preserves unstarted item state")
    {
        const QueueItem &qi = lOrb->factory->queue[1];
        CHECK(qi.progress == 0);
        CHECK(qi.started == false);
        CHECK(qi.repeat == false);
    }

    removeSaveFile();
}

TEST_CASE("SaveGame round-trips research facility current_project")
{
    Game *game = createTestGame();
    REQUIRE(game != nullptr);
    REQUIRE(game->researchTopics.size() >= 1);

    EarthCity *ec = nullptr;
    for (ResourceFacility *b : game->allBases())
    {
        if (b->type == LOCATION_TYPE_EARTH_CITY)
        {
            ec = static_cast<EarthCity *>(b);
            break;
        }
    }
    REQUIRE(ec != nullptr);
    REQUIRE(ec->research_facility != nullptr);
    ec->research_facility->current_project = 0;

    SaveGame saver;
    REQUIRE(saver.save(SAVE_PATH) == 0);

    Game loaded;
    Loader loader(SAVE_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(loaded.initialise(&loader));

    EarthCity *loadedEc = nullptr;
    for (ResourceFacility *b : loaded.allBases())
    {
        if (b->type == LOCATION_TYPE_EARTH_CITY)
        {
            loadedEc = static_cast<EarthCity *>(b);
            break;
        }
    }
    REQUIRE(loadedEc != nullptr);
    REQUIRE(loadedEc->research_facility != nullptr);
    CHECK(loadedEc->research_facility->current_project == 0);

    removeSaveFile();
}

TEST_CASE("SaveGame omits idle research facilities from the save file")
{
    // current_project == -1 (idle) is the default; skipping the insert keeps
    // the table sparse, matching the convention used by saveStores for
    // zero-amount rows.
    Game *game = createTestGame();
    REQUIRE(game != nullptr);

    SaveGame saver;
    REQUIRE(saver.save(SAVE_PATH) == 0);

    Loader verify(SAVE_PATH);
    REQUIRE(verify.isValid());
    SQLiteQuery q(&verify, "SELECT count(*) FROM research_facilities");
    REQUIRE(q.next());
    CHECK(sqlite3_column_int(q, 0) == 0);

    removeSaveFile();
}

TEST_CASE("SaveGame round-trips craft, pods, destinations, and autopilot")
{
    Game *game = createTestGame();
    REQUIRE(game != nullptr);

    Location *earth = game->locationByID(4);
    REQUIRE(earth != nullptr);
    Orbital *orb = game->orbitalAt(earth);
    REQUIRE(orb != nullptr);

    // --- Shuttle: docked, drive fitted, pods loaded, autopilot AS_ON, varied flow + cursors
    // Created AT the orbital, so "docked" is where it is rather than what a state says.
    Shuttle *shuttle = game->createShuttle(orb);
    REQUIRE(shuttle != nullptr);

    std::snprintf(shuttle->name, sizeof shuttle->name, "Discovery");
    shuttle->assignState(CS_IDLE, 0.0f, 0.0f); // at rest; docked comes from location
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

    // Two genuinely different locations at one body: the station on the ground, and
    // the orbit region with no docking implied. That distinction used to need a side
    // and a docked flag alongside a single shared body.
    REQUIRE(game->resourceFacilityAt(earth) != nullptr);
    REQUIRE(earth->orbit() != nullptr);
    shuttle->destinations[0] = Endpoint(game->resourceFacilityAt(earth));
    shuttle->destinations[1] = Endpoint(earth->orbit());

    shuttle->autopilot->state = AS_ON;
    for (int i = 0; i < ResourceType::Count; ++i)
    {
        shuttle->autopilot->flow[i] = 0;
    }
    shuttle->autopilot->flow[ResourceType::Iron] = RF_LOAD_AT_SOURCE; // first non-trivial index
    shuttle->autopilot->flow[ResourceType::Copper] = RF_BALANCE;
    shuttle->autopilot->flow[ResourceType::HedFuel] = RF_LOAD_AT_DEST; // boundary: ResourceType::Count - 1 (= 16)
    shuttle->autopilot->cursors[0] = 5;
    shuttle->autopilot->cursors[1] = 12;

    // --- IOS: caught MID-MANOEUVRE, autopilot AS_COMPLETE. The partial timer is the
    // point: state_timer and total_state_timer differ, which is the one shape neither
    // setState nor setTimedState can express and so the one assignState exists for.
    IOS *ios = game->createIOS(static_cast<Location *>(orb));
    REQUIRE(ios != nullptr);
    std::snprintf(ios->name, sizeof ios->name, "IOS-Test");
    ios->location = orb->body()->orbit(); // approaching the orbital from the region
    ios->assignState(CS_DOCKING, 1.5f, 3.0f);
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
    Shuttle *ls = lShuttles[0].get();

    const auto &lIOS = loaded.allIOS();
    REQUIRE(lIOS.size() == 1);
    IOS *li = lIOS[0].get();

    SUBCASE("round-trips shuttle fields")
    {
        CHECK_STREQ(ls->name, "Discovery");
        CHECK(ls->type == CT_SHUTTLE);
        const CurrentState cs = ls->currentState();
        CHECK(cs.state == CS_IDLE);
        CHECK(cs.state_timer == doctest::Approx(0.0f));
        CHECK(cs.total_state_timer == doctest::Approx(0.0f));
        CHECK(ls->docked()); // and the docked half survives as the location
        CHECK(ls->fuel == 100);
        CHECK(ls->max_pods == 2);
        CHECK(ls->drive == true);
        CHECK(ls->destination_index == 1);
        REQUIRE(ls->location != nullptr);
        CHECK_STREQ(ls->location->name, "Earth Orbital"); // the exact place, not the body
    }

    SUBCASE("round-trips IOS fields")
    {
        CHECK_STREQ(li->name, "IOS-Test");
        CHECK(li->type == CT_IOS);
        const CurrentState cs = li->currentState();
        CHECK(cs.state == CS_DOCKING);
        CHECK(cs.state_timer == doctest::Approx(1.5f));
        CHECK(cs.total_state_timer == doctest::Approx(3.0f));
        CHECK(li->stateProgress() == doctest::Approx(0.5f)); // 1.5 of 3.0 remaining
        CHECK_FALSE(li->docked());                           // still on approach
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
        // The location id is the whole record now, so a round trip has to bring back
        // the exact places -- a surface station and an orbit region, both at Earth.
        REQUIRE(ls->destinations[0].location != nullptr);
        CHECK(ls->destinations[0].location->isFacility());
        CHECK_FALSE(ls->destinations[0].location->inOrbit());
        CHECK_STREQ(ls->destinations[0].location->body()->name, "Earth");

        REQUIRE(ls->destinations[1].location != nullptr);
        CHECK(ls->destinations[1].location->type == LOCATION_TYPE_ORBIT);
        CHECK_STREQ(ls->destinations[1].location->body()->name, "Earth");
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

        Location *earth = game->locationByID(4);
        REQUIRE(earth != nullptr);
        Orbital *orb = game->orbitalAt(earth);
        REQUIRE(orb != nullptr);

        Shuttle *shuttle = game->createShuttle(orb->body());
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

TEST_CASE("facility type is stored, not inferred from its contents")
{
    // Regression for the save-time inference that used to live in saveBase:
    //   if (rf->training_facility || rf->research_facility) type = EARTH_CITY;
    // Game::createResearchFacility accepts ANY ResourceFacility, so attaching one to
    // a plain surface facility made it save as an Earth City and load back as an
    // EarthCity object -- a round trip that changed the object's class.
    Game *game = Game::createCurrent();
    Loader loader(DB_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(game->initialise(&loader));

    // Luna (id 5) has a plain resource facility, distinct from Earth's Earth City.
    Location *luna = game->locationByID(5);
    REQUIRE(luna != nullptr);
    ResourceFacility *rf = game->resourceFacilityAt(luna);
    REQUIRE(rf != nullptr);
    REQUIRE(rf->type == LOCATION_TYPE_RESOURCE_FACILITY);
    REQUIRE_FALSE(rf->isEarthCity());

    // The trigger: a research facility on an ordinary resource facility.
    REQUIRE(game->createResearchFacility(rf) != nullptr);
    REQUIRE(rf->research_facility != nullptr);

    SaveGame saver;
    REQUIRE(saver.save(SAVE_PATH) == 0);

    Game &loaded = *Game::createCurrent();
    Loader reloader(SAVE_PATH);
    REQUIRE(reloader.isValid());
    REQUIRE(loaded.initialise(&reloader));

    Location *loadedLuna = loaded.locationByID(5);
    REQUIRE(loadedLuna != nullptr);
    ResourceFacility *loadedRf = loaded.resourceFacilityAt(loadedLuna);
    REQUIRE(loadedRf != nullptr);

    // Still a resource facility, and still not an Earth City.
    CHECK(loadedRf->type == LOCATION_TYPE_RESOURCE_FACILITY);
    CHECK_FALSE(loadedRf->isEarthCity());
    CHECK(loadedRf->training_facility == nullptr);
    CHECK_FALSE(loadedRf->inOrbit());

    // and the Earth City is still itself
    Location *earth = loaded.locationByID(4);
    REQUIRE(earth != nullptr);
    ResourceFacility *ec = loaded.resourceFacilityAt(earth);
    REQUIRE(ec != nullptr);
    CHECK(ec->type == LOCATION_TYPE_EARTH_CITY);
    CHECK(ec->isEarthCity());
    CHECK(ec->training_facility != nullptr);
    CHECK_FALSE(ec->inOrbit());

    removeSaveFile();
}

TEST_CASE("a destination resolves a body to an exact place")
{
    // The picker offers bodies; the endpoint has to name somewhere precise. This is
    // what replaced the old "pick a body, then set a side and a docked flag".
    Game *game = Game::createCurrent();
    Loader loader(DB_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(game->initialise(&loader));

    Location *earth = game->locationByID(4);
    REQUIRE(earth != nullptr);
    Orbital *earthOrbital = game->orbitalAt(earth);
    REQUIRE(earthOrbital != nullptr);

    SUBCASE("a body with a station resolves to the station")
    {
        CHECK(game->targetFor(earth, true) == earthOrbital);
    }

    SUBCASE("a body without one resolves to its orbit region")
    {
        // Luna has a surface station but no orbital.
        Location *luna = game->locationByID(5);
        REQUIRE(luna != nullptr);
        REQUIRE(game->orbitalAt(luna) == nullptr);
        REQUIRE(luna->orbit() != nullptr);
        CHECK(game->targetFor(luna, true) == luna->orbit());
    }

    SUBCASE("something already exact is left alone")
    {
        CHECK(game->targetFor(earthOrbital, true) == earthOrbital);
    }

    SUBCASE("a body with no regions resolves to itself")
    {
        // An asteroid belt has nothing to orbit or land on, so the belt IS the exact
        // place. Resolving through a region returned null, which made the belt
        // unreachable: setDestination produced a null endpoint, and the autopilot then
        // reported CAC_NO_DESTINATION for the one place mining happens.
        Location *belt = game->locationByID(9);
        REQUIRE(belt != nullptr);
        REQUIRE(belt->type == LOCATION_TYPE_ASTEROID_BELT);
        REQUIRE(belt->orbit() == nullptr);
        REQUIRE(belt->surface() == nullptr);

        CHECK(game->targetFor(belt, true) == belt);
        CHECK(game->targetFor(belt, false) == belt); // neither side exists, so both agree

        // A system's `space` is region-less for the same reason.
        Location *space = game->locationByID(0);
        REQUIRE(space != nullptr);
        REQUIRE(space->type == LOCATION_TYPE_SPACE);
        CHECK(game->targetFor(space, true) == space);
    }

    SUBCASE("a region-less body can be set as a destination")
    {
        // What the resolution is for: the endpoint has to name somewhere a craft can
        // actually be sent.
        Location *belt = game->locationByID(9);
        REQUIRE(belt != nullptr);

        IOS *ios = game->createIOS(static_cast<Location *>(earthOrbital));
        REQUIRE(ios != nullptr);
        ios->drive = true;
        ios->setDestination(0, belt);

        REQUIRE_MESSAGE(ios->destinations[0].location != nullptr, "the belt is unreachable");
        CHECK(ios->destinations[0].location == belt);
    }
}

TEST_CASE("atEndpoint compares locations")
{
    Game *game = Game::createCurrent();
    Loader loader(DB_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(game->initialise(&loader));

    Location *earth = game->locationByID(4);
    REQUIRE(earth != nullptr);
    REQUIRE(earth->orbit() != nullptr);
    Orbital *orbital = game->orbitalAt(earth);
    REQUIRE(orbital != nullptr);

    Shuttle *shuttle = earth->shuttle ? earth->shuttle : game->createShuttle(earth);
    REQUIRE(shuttle != nullptr);
    shuttle->destination_index = 0;

    SUBCASE("the same location is arrival")
    {
        shuttle->destinations[0] = Endpoint(orbital);
        shuttle->location = orbital;
        CHECK(shuttle->atEndpoint());
    }

    SUBCASE("in orbit is not the same as docked")
    {
        shuttle->destinations[0] = Endpoint(orbital);
        shuttle->location = earth->orbit();
        CHECK_FALSE(shuttle->atEndpoint());

        shuttle->destinations[0] = Endpoint(earth->orbit());
        CHECK(shuttle->atEndpoint());
    }

    SUBCASE("docked inside the region we were sent to counts")
    {
        // Descending auto-docks when a station is there, so aiming at the region and
        // ending up in its station is arrival, not an overshoot.
        shuttle->destinations[0] = Endpoint(earth->orbit());
        shuttle->location = orbital;
        CHECK(shuttle->atEndpoint());
    }

    SUBCASE("another body is never the endpoint")
    {
        Location *luna = game->locationByID(5);
        REQUIRE(luna != nullptr);
        REQUIRE(luna->orbit() != nullptr);
        shuttle->destinations[0] = Endpoint(luna->orbit());
        shuttle->location = earth->orbit();
        CHECK_FALSE(shuttle->atEndpoint());
    }
}

TEST_CASE("SaveGame round-trips item work definitions")
{
    // The table is sparse on purpose: a row exists only for cargo that can be worked, so
    // `does_work` reads presence rather than a zero sentinel. A save that dropped the rows
    // would silently give every working item a zero work_time -- which sets a CS_WORKING
    // timer of 0 that never expires, leaving the craft working forever.
    Game *game = createTestGame();
    REQUIRE(game != nullptr);
    REQUIRE(game->items.size() > ItemType::R_Frame);

    int definedBefore = 0;
    for (const auto &item : game->items)
    {
        if (item.does_work)
        {
            ++definedBefore;
            const bool hasDuration = item.work_parameters.work_time > 0.0f;
            CHECK_MESSAGE(hasDuration, "work row with no duration: " << item.name);
        }
    }
    CHECK(definedBefore > 0);

    const WorkParameters frame = game->items[ItemType::Of_Frame].work_parameters;
    REQUIRE(game->items[ItemType::Of_Frame].does_work);

    SaveGame saver;
    REQUIRE(saver.save(SAVE_PATH) == 0);

    Game loaded;
    Loader loader(SAVE_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(loaded.initialise(&loader));

    REQUIRE(loaded.items.size() == game->items.size());

    int definedAfter = 0;
    for (const auto &item : loaded.items)
    {
        if (item.does_work)
        {
            ++definedAfter;
        }
    }
    CHECK(definedAfter == definedBefore); // sparseness survives: no rows gained or lost

    const Item &reloaded = loaded.items[ItemType::Of_Frame];
    CHECK(reloaded.does_work);
    CHECK(reloaded.work_parameters.work_time == doctest::Approx(frame.work_time));
    CHECK(reloaded.work_parameters.consumption == frame.consumption);
    CHECK(reloaded.work_parameters.abort_consumes == frame.abort_consumes);
    CHECK(reloaded.work_parameters.auto_continue == frame.auto_continue);

    // and an item with no row stays without one
    CHECK_FALSE(loaded.items[ItemType::S_Chassis].does_work);

    removeSaveFile();
}

TEST_CASE("SaveGame round-trips objects and the references into them")
{
    // Objects are pointers at runtime and ids in the file. Both directions have to agree,
    // and the references are the fragile half: a craft's scan target and a grapple's held
    // object both resolve through objectByID, so an objects table that saved empty would
    // reload them as null with nothing to say so.
    Game *game = createTestGame();
    REQUIRE(game != nullptr);

    Location *earth = game->locationByID(4);
    REQUIRE(earth != nullptr);
    REQUIRE(earth->orbit() != nullptr);

    // A persistent one attached to a location, and a transient one being scanned.
    Object *artefact = game->createObject(0, ObjectType::Artefact, earth, 1, 0);
    Object *asteroid = game->createObject(0, ObjectType::Asteroid, earth->orbit(), 40, ResourceType::Iron);
    REQUIRE(artefact != nullptr);
    REQUIRE(asteroid != nullptr);
    CHECK(artefact->id != asteroid->id); // ids are allocated, never shared

    Orbital *orb = game->orbitalAt(earth);
    REQUIRE(orb != nullptr);
    IOS *ios = game->createIOS(static_cast<Location *>(orb));
    REQUIRE(ios != nullptr);
    ios->scan_object = asteroid; // currently scanning one
    ios->setPodType(0, PT_TOOL);
    ios->pods[0].contentType = ItemType::Grapple;
    ios->pods[0].amount = 1;
    ios->pods[0].object = artefact; // and holding the other in the grapple

    const int artefactId = artefact->id;
    const int asteroidId = asteroid->id;
    const int iosId = ios->id;
    const size_t objectCount = game->allObjects().size();
    const int orbitId = earth->orbit()->id;

    SaveGame saver;
    REQUIRE(saver.save(SAVE_PATH) == 0);

    Game loaded;
    Loader loader(SAVE_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(loaded.initialise(&loader));

    CHECK_MESSAGE(loaded.allObjects().size() == objectCount, "objects were not saved");

    Object *lAsteroid = loaded.objectByID(asteroidId);
    REQUIRE(lAsteroid != nullptr);
    CHECK(lAsteroid->type == ObjectType::Asteroid);
    CHECK(lAsteroid->quantity == 40);
    CHECK(lAsteroid->resource_id == ResourceType::Iron);
    REQUIRE(lAsteroid->location != nullptr);
    CHECK(lAsteroid->location->id == orbitId); // location is an id in the file, a pointer here

    IOS *lIos = nullptr;
    for (auto &c : loaded.allIOS())
    {
        if (c->id == iosId)
        {
            lIos = c.get();
        }
    }
    REQUIRE(lIos != nullptr);

    // The references, which is the point: pointers again, and to the right objects.
    REQUIRE_MESSAGE(lIos->scan_object != nullptr, "scan target lost across save");
    CHECK(lIos->scan_object == lAsteroid);
    REQUIRE_MESSAGE(lIos->pods[0].object != nullptr, "grappled object lost across save");
    CHECK(lIos->pods[0].object->id == artefactId);

    // an empty pod still references nothing -- 0 in the file must not resolve to an object
    CHECK(lIos->pods[1].object == nullptr);

    removeSaveFile();
}

TEST_CASE("SaveGame round-trips crews and everything that references them")
{
    // Crew is referenced from three places -- a facility's factory, a research facility,
    // and a craft -- each as a pointer at runtime and an id in the file. The table and
    // the three references were added one at a time and hid each other's absence: an
    // unsaved table makes a bad column name unreachable, which makes a load-order bug
    // invisible. Assert the whole chain at once so that cannot happen again.
    Game *game = createTestGame();
    REQUIRE(game != nullptr);

    Location *earth = game->locationByID(4);
    REQUIRE(earth != nullptr);
    ResourceFacility *ec = game->resourceFacilityAt(earth);
    Orbital *orb = game->orbitalAt(earth);
    REQUIRE(ec != nullptr);
    REQUIRE(orb != nullptr);
    REQUIRE(ec->research_facility != nullptr);

    Crew *engineers = game->createCrew(0, CrewType::Engineer, "Kowalski", 2, 6, 12.5f);
    Crew *scientists = game->createCrew(0, CrewType::Scientist, "Vance", 1, 3, 4.0f);
    Crew *marines = game->createCrew(0, CrewType::Marine, "Shepard", 3, 12, 30.0f);
    REQUIRE(engineers != nullptr);
    REQUIRE(scientists != nullptr);
    REQUIRE(marines != nullptr);
    CHECK(engineers->id != scientists->id); // allocated, not shared

    IOS *ios = game->createIOS(static_cast<Location *>(orb));
    REQUIRE(ios != nullptr);

    // one crew per reference site
    CHECK(orb->assignCrew(engineers) == nullptr); // returns the displaced crew, if any
    ec->research_facility->assignCrew(scientists);
    ios->assignCrew(marines);

    const int engineersId = engineers->id;
    const int scientistsId = scientists->id;
    const int marinesId = marines->id;
    const int orbId = orb->id;
    const int ecId = ec->id;
    const int iosId = ios->id;
    const size_t crewCount = game->allCrews().size();

    SaveGame saver;
    REQUIRE(saver.save(SAVE_PATH) == 0);

    Game loaded;
    Loader loader(SAVE_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(loaded.initialise(&loader));

    CHECK_MESSAGE(loaded.allCrews().size() == crewCount, "crews were not saved");

    Crew *lEngineers = loaded.crewByID(engineersId);
    REQUIRE_MESSAGE(lEngineers != nullptr, "crew row lost across save");
    CHECK(lEngineers->type == CrewType::Engineer);
    CHECK_STREQ(lEngineers->leader_name, "Kowalski"); // the column name mismatch lands here
    CHECK(lEngineers->rank == 2);
    CHECK(lEngineers->size == 6);
    CHECK(lEngineers->experience == doctest::Approx(12.5f));

    // The three references, which is what the load order decides.
    Orbital *lOrb = static_cast<Orbital *>(loaded.locationByID(orbId));
    REQUIRE(lOrb != nullptr);
    REQUIRE_MESSAGE(lOrb->factory_crew != nullptr, "facility crew lost -- crews must load before facilities");
    CHECK(lOrb->factory_crew == lEngineers);

    ResourceFacility *lEc = static_cast<ResourceFacility *>(loaded.locationByID(ecId));
    REQUIRE(lEc != nullptr);
    REQUIRE(lEc->research_facility != nullptr);
    REQUIRE_MESSAGE(lEc->research_facility->crew != nullptr, "research crew lost across save");
    CHECK(lEc->research_facility->crew->id == scientistsId);

    IOS *lIos2 = nullptr;
    for (auto &c : loaded.allIOS())
    {
        if (c->id == iosId)
        {
            lIos2 = c.get();
        }
    }
    REQUIRE(lIos2 != nullptr);
    REQUIRE_MESSAGE(lIos2->crew != nullptr, "craft crew lost across save");
    CHECK(lIos2->crew->id == marinesId);

    // and an unassigned craft still has none -- 0 must not resolve to a crew
    Shuttle *anyShuttle = earth->shuttle;
    if (anyShuttle)
    {
        CHECK(anyShuttle->crew == nullptr);
    }

    removeSaveFile();
}
