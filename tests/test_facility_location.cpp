// Stage 2 of docs/plans/facilities_as_locations.md: a Facility IS a Location.
//
// These pin the properties that make that true -- parent linkage, identity drawn
// from the location sequence, a resolved position near the parent, and invisibility
// in the orrery until the rendering work is done deliberately.

#include "doctest.h"
#include "../include/loaders/loader.h"
#include "../include/loaders/save_game.h"
#include "../include/state/game.h"
#include "../include/state/system.h"
#include "../include/state/location.h"
#include "../include/state/facility.h"
#include "../include/state/resourceFacility.h"
#include "../include/state/orbital.h"

#include <cmath>
#include <cstdio>
#include <set>

namespace
{

const char *FL_DB_PATH = "./resources/initial.db";
const int EARTH_ID = 4;

float distanceBetween(const Location *a, const Location *b)
{
    const Vector2 pa = a->resolvedPosition();
    const Vector2 pb = b->resolvedPosition();
    return std::sqrt((pa.x - pb.x) * (pa.x - pb.x) + (pa.y - pb.y) * (pa.y - pb.y));
}

Game *loadGame()
{
    Game *game = Game::createCurrent();
    Loader loader(FL_DB_PATH);
    if (!loader.isValid() || !game->initialise(&loader))
    {
        return nullptr;
    }
    return game;
}

} // namespace

TEST_CASE("facilities are child locations of their body")
{
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *earth = game->locationByID(EARTH_ID);
    REQUIRE(earth != nullptr);

    Orbital *orbital = game->orbitalAt(earth);
    REQUIRE(orbital != nullptr);

    CHECK(orbital->primary == earth);
    CHECK(orbital->system == earth->system);
    CHECK(orbital->isFacility());
    CHECK(orbital->type == LOCATION_TYPE_ORBITAL);

    // it is in the parent's child list, and resolvable as a location
    bool found = false;
    for (Location *child : earth->children)
    {
        if (child == orbital)
        {
            found = true;
        }
    }
    CHECK(found);
    CHECK(game->locationByID(orbital->id) == orbital);

    // and in the system's location list, so System::update moves it
    bool inSystem = false;
    for (Location *loc : earth->system->locations)
    {
        if (loc == orbital)
        {
            inSystem = true;
        }
    }
    CHECK(inSystem);
}

TEST_CASE("facility ids extend the body sequence and stay unique")
{
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    int highestBody = -1;
    for (auto &loc : game->allLocations())
    {
        if (!loc->isFacility() && loc->id > highestBody)
        {
            highestBody = loc->id;
        }
    }
    REQUIRE(highestBody > 0);

    // Facility ids used to start at 1 and collide with body ids.
    std::set<int> seen;
    int facilityCount = 0;
    for (auto &loc : game->allLocations())
    {
        CHECK(seen.insert(loc->id).second); // unique across every location
        if (loc->isFacility())
        {
            ++facilityCount;
            CHECK(loc->id > highestBody);
        }
    }
    CHECK(facilityCount > 0);
    CHECK(game->location_max_id >= highestBody);
}

TEST_CASE("facility positions resolve relative to their parent")
{
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *earth = game->locationByID(EARTH_ID);
    REQUIRE(earth != nullptr);
    for (auto &sys : game->allSystems())
    {
        sys->update(1000.0f);
    }

    Orbital *orbital = game->orbitalAt(earth);
    REQUIRE(orbital != nullptr);

    // Stands off the body: a distinct position for transit maths, but close enough
    // that reaching the station and reaching the body cost about the same.
    const float standoff = distanceBetween(orbital, earth);
    CHECK(standoff > 0.0f);
    CHECK(standoff < 10.0f);

    // A surface facility sits at the body itself.
    ResourceFacility *surface = game->resourceFacilityAt(earth);
    REQUIRE(surface != nullptr);
    CHECK(distanceBetween(surface, earth) == doctest::Approx(0.0f));
}

TEST_CASE("facilities are not drawn or hit-tested in the orrery")
{
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    // radius 0 is what keeps them out of both the draw loop and mouseOverBody,
    // the same treatment the asteroid belt and space bodies already get.
    int checked = 0;
    for (auto &loc : game->allLocations())
    {
        if (loc->isFacility())
        {
            CHECK(loc->radius == doctest::Approx(0.0f));
            ++checked;
        }
    }
    CHECK(checked > 0);
}

TEST_CASE("siblings at one body get distinct positions")
{
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *earth = game->locationByID(EARTH_ID);
    REQUIRE(earth != nullptr);

    Orbital *first = game->orbitalAt(earth);
    REQUIRE(first != nullptr);

    // A second orbital at the same body -- the multiplicity future.md plans for.
    Orbital *second = game->createOrbital(earth);
    REQUIRE(second != nullptr);
    CHECK(second != first);
    CHECK(second->primary == earth);
    CHECK(second->id != first->id);

    for (auto &sys : game->allSystems())
    {
        sys->update(1000.0f);
    }

    CHECK(second->initial_angle != doctest::Approx(first->initial_angle));
    CHECK(distanceBetween(first, second) > 0.0f);
}

TEST_CASE("facilities round-trip as locations")
{
    const char *FL_SAVE_PATH = "./test_facility_location.db";

    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *earth = game->locationByID(EARTH_ID);
    REQUIRE(earth != nullptr);
    Orbital *orbital = game->orbitalAt(earth);
    REQUIRE(orbital != nullptr);

    const int orbitalId = orbital->id;
    const int parentId = orbital->primary->id;
    const size_t locationCount = game->allLocations().size();

    SaveGame saver;
    REQUIRE(saver.save(FL_SAVE_PATH) == 0);

    Game *loaded = Game::createCurrent();
    Loader reloader(FL_SAVE_PATH);
    REQUIRE(reloader.isValid());
    REQUIRE(loaded->initialise(&reloader));

    // Identity survives: a facility id is a location id, no longer renumbered from
    // 1 on every save.
    Location *reloadedLoc = loaded->locationByID(orbitalId);
    REQUIRE(reloadedLoc != nullptr);
    CHECK(reloadedLoc->isFacility());
    CHECK(reloadedLoc->type == LOCATION_TYPE_ORBITAL);
    CHECK(reloadedLoc->primary != nullptr);
    CHECK(reloadedLoc->primary->id == parentId);

    // and it is the same object the facility accessor returns
    Location *reloadedEarth = loaded->locationByID(EARTH_ID);
    REQUIRE(reloadedEarth != nullptr);
    CHECK(loaded->orbitalAt(reloadedEarth) == reloadedLoc);

    // no duplication: facilities are written to `facilities`, never to `bodies`
    CHECK(loaded->allLocations().size() == locationCount);

    std::remove(FL_SAVE_PATH);
}

TEST_CASE("a shuttle's owner and its position are separate")
{
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *earth = game->locationByID(EARTH_ID);
    REQUIRE(earth != nullptr);
    Orbital *orbital = game->orbitalAt(earth);
    REQUIRE(orbital != nullptr);

    // Create it AT the orbital -- the case that matters once a docked craft's
    // location is the facility. Ownership must still land on the body, or a save
    // and reload reparents the shuttle somewhere no reader looks.
    //
    // The cast selects createShuttle(Location*) deliberately. An Orbital* binds to
    // the Facility* overload instead (fewer base conversions), which means "a
    // shuttle belonging to this facility's body" and sets up its route -- a
    // different operation that happens to share a name.
    Shuttle *s = game->createShuttle(static_cast<Location *>(orbital));
    REQUIRE(s != nullptr);

    CHECK(s->location == orbital);       // position: where it is
    CHECK(earth->shuttle == s);          // reference: on the body
    CHECK(orbital->shuttle == nullptr);  // never on the facility

    // Game holds the lifetime, as it does for IOS
    bool owned = false;
    for (auto &owned_shuttle : game->allShuttles())
    {
        if (owned_shuttle.get() == s)
        {
            owned = true;
        }
    }
    CHECK(owned);

    // one shuttle per body still holds, whichever location it is asked for
    CHECK(game->createShuttle(earth) == nullptr);
    CHECK(game->createShuttle(static_cast<Location *>(orbital)) == nullptr);
}
