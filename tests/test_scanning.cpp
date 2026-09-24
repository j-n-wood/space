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
#include "../include/state/resources.h"

#include <string>

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
    // A belt has no orbit or surface children -- there is nothing to land on -- so the belt
    // itself is the place a craft occupies.
    IOS *iosAtBelt(Game *game, Location *belt)
    {
        IOS *ios = game->createIOS(belt);
        if (!ios)
        {
            return nullptr;
        }
        ios->drive = true;
        ios->fuel = 250;
        ios->location = belt;
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
}

TEST_CASE("leaving the scan state keeps the target")
{
    // Deliberate: stopping the scan is not the same as discarding what was found. A craft
    // that stops scanning to go and WORK on the asteroid it just located would otherwise
    // arrive with nothing to act on. Only departure collects the target -- see below.
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *belt = game->locationByID(BELT_ID);
    REQUIRE(belt != nullptr);
    REQUIRE(belt->type == LOCATION_TYPE_ASTEROID_BELT);
    REQUIRE_MESSAGE(belt->orbit() == nullptr, "a belt has no sub-locations to orbit or land on");

    IOS *ios = iosAtBelt(game, belt);
    REQUIRE(ios != nullptr);

    Object *rock = game->createObject(0, ObjectType::Asteroid, ios->location, 50, ResourceType::Iron);
    REQUIRE(rock != nullptr);
    ios->scan_object = rock;
    ios->startScanning();
    const int before = countObjects(game, ObjectType::Asteroid);
    REQUIRE(before > 0);

    SUBCASE("stopScanning leaves it in hand")
    {
        ios->stopScanning();

        CHECK(ios->currentState().state == CS_IDLE);
        CHECK_MESSAGE(ios->scan_object == rock, "the located asteroid was discarded");
        CHECK(countObjects(game, ObjectType::Asteroid) == before);
    }

    SUBCASE("so does a manoeuvre that ends the scan")
    {
        // Any transition out of CS_SCANNING goes through the same hook, so starting work
        // on the target must not take the target away.
        ios->work(1.0f);

        CHECK(ios->working());
        CHECK(ios->scan_object == rock);
        CHECK(countObjects(game, ObjectType::Asteroid) == before);
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
    ios->setDestination(0, earth);

    SUBCASE("a transient asteroid is collected")
    {
        Object *rock = game->createObject(0, ObjectType::Asteroid, ios->location, 50, ResourceType::Iron);
        REQUIRE(rock != nullptr);
        ios->scan_object = rock;
        const int before = countObjects(game, ObjectType::Asteroid);

        REQUIRE(bool(ios->canEngageDrive()));
        ios->engageDrive();

        REQUIRE(ios->inTransit());
        CHECK(ios->scan_object == nullptr);
        CHECK_MESSAGE(countObjects(game, ObjectType::Asteroid) == before - 1,
                      "asteroid left behind at a belt the craft has departed");
    }

    SUBCASE("a persistent object is kept")
    {
        // Story items exist whether or not anyone is scanning, so departing must drop the
        // reference without destroying the thing referenced.
        Object *artefact = game->createObject(0, ObjectType::Artefact, ios->location, 1, 0);
        REQUIRE(artefact != nullptr);
        const int artefactId = artefact->id;
        ios->scan_object = artefact;

        REQUIRE(bool(ios->canEngageDrive()));
        ios->engageDrive();

        REQUIRE(ios->inTransit());
        CHECK(ios->scan_object == nullptr); // no longer scanning it
        CHECK_MESSAGE(game->objectByID(artefactId) != nullptr, "a story item was collected as garbage");
    }
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

TEST_CASE("scan target text names the target")
{
    // The UI reads this every frame while scanning. It is all printf formatting, which
    // nothing else exercises -- quantity is an int, and a %f for it read an unset
    // floating-point register rather than the tonnage.
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *belt = game->locationByID(BELT_ID);
    REQUIRE(belt != nullptr);

    IOS *ios = iosAtBelt(game, belt);
    REQUIRE(ios != nullptr);

    char buf[128];

    SUBCASE("nothing scanned")
    {
        ios->scan_object = nullptr;
        CHECK(std::string(ios->scanTargetText(buf, sizeof buf)) == std::string("No scan target"));
    }

    SUBCASE("an asteroid names its tonnage and resource")
    {
        Object *rock = game->createObject(0, ObjectType::Asteroid, belt, 40, ResourceType::Iron);
        REQUIRE(rock != nullptr);
        ios->scan_object = rock;

        const std::string text = ios->scanTargetText(buf, sizeof buf);
        CHECK(text == std::string("asteroid: 40 ") + ResourceName[ResourceType::Iron]);
    }

    SUBCASE("an artefact names itself")
    {
        Object *artefact = game->createObject(0, ObjectType::Artefact, belt, 1, 0);
        REQUIRE(artefact != nullptr);
        ios->scan_object = artefact;
        CHECK(std::string(ios->scanTargetText(buf, sizeof buf)) == std::string("artefact"));
    }
}

TEST_CASE("a grapple pod describes what it holds")
{
    // Pod::description switches to the held object when there is one, so the bay and
    // cockpit show cargo rather than the tool name.
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *belt = game->locationByID(BELT_ID);
    REQUIRE(belt != nullptr);
    IOS *ios = iosAtBelt(game, belt);
    REQUIRE(ios != nullptr);

    ios->setPodType(0, PT_TOOL);
    ios->pods[0].contentType = ItemType::Grapple;
    ios->pods[0].amount = 1;

    char buf[128];

    // empty grapple: the tool names itself
    ios->pods[0].object = nullptr;
    CHECK(std::string(ios->pods[0].description(buf, sizeof buf)) ==
          std::string(game->items[ItemType::Grapple].name));

    // holding something: the cargo names itself
    Object *rock = game->createObject(0, ObjectType::Asteroid, belt, 40, ResourceType::Iron);
    REQUIRE(rock != nullptr);
    ios->pods[0].object = rock;
    CHECK(std::string(ios->pods[0].description(buf, sizeof buf)) == std::string("Grapple: Asteroid"));
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
        game->update(0.05);
    }

    CHECK_MESSAGE(ios->currentState().state == CS_SCANNING,
                  "scanning stopped instead of rotating to the next target");
    CHECK(ios->scan_object != nullptr);
    CHECK(ios->currentState().state_timer > 0.0f);
}
