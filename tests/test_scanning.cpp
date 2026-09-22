// Craft scanning: CS_SCANNING, scan targets, and the objects behind them.
//
// Scanning is a craft state rather than a UI mode, so it survives leaving the panel, and a
// scan target is an Object referenced by pointer. Two properties carry most of the weight:
// the state is the only thing that rotates the target, and leaving the state must release
// a transient target rather than orphan it.

#include "doctest.h"
#include "../include/loaders/loader.h"
#include "../include/state/game.h"
#include "../include/state/system.h"
#include "../include/state/location.h"
#include "../include/state/facility.h"
#include "../include/state/resourceFacility.h"
#include "../include/state/orbital.h"
#include "../include/state/shuttle.h"
#include "../include/state/ios.h"
#include "../include/state/object.h"
#include "../include/state/autopilot.h"

namespace
{

const char *SC_DB_PATH = "./resources/initial.db";
const int EARTH_ID = 4;
const int BELT_ID = 9; // Sol's asteroid belt, the only LOCATION_TYPE_ASTEROID_BELT body

Game *loadGame()
{
    Game *game = Game::createCurrent();
    Loader loader(SC_DB_PATH);
    if (!loader.isValid() || !game->initialise(&loader))
    {
        return nullptr;
    }
    return game;
}

// An IOS in the belt's orbit region, at rest and able to manoeuvre.
IOS *iosAtBelt(Game *game, Location *belt)
{
    IOS *ios = game->createIOS(belt->orbit());
    if (!ios)
    {
        return nullptr;
    }
    ios->drive = true;
    ios->fuel = 250;
    ios->location = belt->orbit();
    ios->assignState(CS_IDLE, 0.0f, 0.0f);
    return ios;
}

int countObjects(Game *game, ObjectType type)
{
    int n = 0;
    for (auto &obj : game->allObjects())
    {
        if (obj->type == type)
        {
            ++n;
        }
    }
    return n;
}

} // namespace

TEST_CASE("leaving the scan state does not recurse")
{
    // stopScanning() assigns `state` directly rather than going back through setState().
    // Routing it through the mutator recursed forever -- the mutator calls stopScanning
    // BEFORE assigning, so `state` was still CS_SCANNING on re-entry and nothing
    // terminated. A scanning craft that manoeuvred segfaulted.
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *earth = game->locationByID(EARTH_ID);
    REQUIRE(earth != nullptr);
    REQUIRE(earth->surface() != nullptr);

    Shuttle *s = earth->shuttle ? earth->shuttle : game->createShuttle(earth);
    REQUIRE(s != nullptr);
    s->drive = true;
    s->location = earth->surface();
    s->assignState(CS_IDLE, 0.0f, 0.0f);

    s->startScanning();
    REQUIRE(s->currentState().state == CS_SCANNING);

    // The crash needed a manoeuvre that is actually permitted: ascend from orbit is
    // refused before it reaches the mutator, which is how this first went unnoticed.
    REQUIRE(bool(s->canAscend()));
    s->ascend();

    CHECK(s->currentState().state == CS_ASCENDING);
    CHECK(s->scan_object == nullptr); // and the target went with the state
}

TEST_CASE("leaving the scan state releases a transient target")
{
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *belt = game->locationByID(BELT_ID);
    REQUIRE(belt != nullptr);
    REQUIRE(belt->type == LOCATION_TYPE_ASTEROID_BELT);

    IOS *ios = iosAtBelt(game, belt);
    REQUIRE(ios != nullptr);

    SUBCASE("an asteroid is dropped, not orphaned")
    {
        Object *rock = game->createObject(0, ObjectType::Asteroid, ios->location, 50, ResourceType::Iron);
        REQUIRE(rock != nullptr);
        ios->scan_object = rock;
        ios->startScanning();
        const int before = countObjects(game, ObjectType::Asteroid);
        REQUIRE(before > 0);

        ios->stopScanning();

        CHECK(ios->scan_object == nullptr);
        CHECK_MESSAGE(countObjects(game, ObjectType::Asteroid) == before - 1,
                      "transient asteroid was orphaned rather than collected");
    }

    SUBCASE("a persistent object is kept")
    {
        // Story items exist whether or not anyone is scanning, so releasing the scan must
        // not take one with it.
        Object *artefact = game->createObject(0, ObjectType::Artefact, ios->location, 1, 0);
        REQUIRE(artefact != nullptr);
        const int artefactId = artefact->id;
        ios->scan_object = artefact;
        ios->startScanning();

        ios->stopScanning();

        CHECK(ios->scan_object == nullptr);        // no longer scanning it
        CHECK(game->objectByID(artefactId) != nullptr); // but it still exists
    }
}

TEST_CASE("engaging the drive releases the scan target")
{
    // The leaving-a-body case. engageDrive is the only way to leave a body, so it is the
    // only place a scan target can be left behind at a belt the craft is no longer at.
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *belt = game->locationByID(BELT_ID);
    Location *earth = game->locationByID(EARTH_ID);
    REQUIRE(belt != nullptr);
    REQUIRE(earth != nullptr);

    IOS *ios = iosAtBelt(game, belt);
    REQUIRE(ios != nullptr);

    Object *rock = game->createObject(0, ObjectType::Asteroid, ios->location, 50, ResourceType::Iron);
    REQUIRE(rock != nullptr);
    ios->scan_object = rock;
    const int before = countObjects(game, ObjectType::Asteroid);

    ios->setDestination(0, earth);
    REQUIRE(bool(ios->canEngageDrive()));
    ios->engageDrive();

    REQUIRE(ios->inTransit());
    CHECK(ios->scan_object == nullptr);
    CHECK(countObjects(game, ObjectType::Asteroid) == before - 1);
}

TEST_CASE("an asteroid takes a resource the belt actually has")
{
    // randomiseAsteroid draws from the location's own availability, so a belt's asteroids
    // reflect its composition rather than a separate table.
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *belt = game->locationByID(BELT_ID);
    REQUIRE(belt != nullptr);

    bool anyAvailable = false;
    for (int idx = 0; idx < ResourceType::Count; ++idx)
    {
        if (belt->resources.availability[idx])
        {
            anyAvailable = true;
        }
    }
    REQUIRE(anyAvailable);

    Object *rock = game->createObject(0, ObjectType::Asteroid, belt, 0, 0);
    REQUIRE(rock != nullptr);

    for (int roll = 0; roll < 20; ++roll)
    {
        REQUIRE(game->randomiseAsteroid(rock) == rock);
        REQUIRE(rock->resource_id >= 0);
        REQUIRE(rock->resource_id < ResourceType::Count);
        CHECK_MESSAGE(belt->resources.availability[rock->resource_id] > 0,
                      "asteroid took a resource the belt does not have");
        CHECK(rock->quantity > 0);
    }
}

TEST_CASE("scanning keeps rotating rather than stopping after one target")
{
    // The self-loop: CS_SCANNING re-arms its own timer, which is what makes the rotation
    // survive a save (state_timer carries the time to the next target) and what freezes
    // the target once work starts.
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *belt = game->locationByID(BELT_ID);
    REQUIRE(belt != nullptr);

    IOS *ios = iosAtBelt(game, belt);
    REQUIRE(ios != nullptr);
    ios->location = belt; // the belt itself is the scannable place

    ios->startScanning();
    REQUIRE(ios->currentState().state == CS_SCANNING);

    // Run well past several intervals. A craft that stops scanning has dropped out of the
    // state; one that is still cycling is in it with a target and a running timer.
    for (int tick = 0; tick < 2000; ++tick)
    {
        game->update(0.05f);
    }

    CHECK_MESSAGE(ios->currentState().state == CS_SCANNING,
                  "scanning stopped instead of rotating to the next target");
    CHECK(ios->scan_object != nullptr);
    CHECK(ios->currentState().state_timer > 0.0f);
}
