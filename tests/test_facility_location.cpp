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
#include "../include/state/shuttle.h"
#include "../include/state/autopilot.h"

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

    // An orbital sits IN the body's orbit region, not directly on the body -- which is
    // what makes "am I in orbit?" an ancestor test rather than a set of types.
    REQUIRE(earth->orbit() != nullptr);
    CHECK(orbital->primary == earth->orbit());
    CHECK(orbital->body() == earth);
    CHECK(orbital->inOrbit());
    CHECK(orbital->system == earth->system);
    CHECK(orbital->isFacility());
    CHECK(orbital->type == LOCATION_TYPE_ORBITAL);

    // it is in the orbit region's child list, and resolvable as a location
    bool found = false;
    for (Location *child : earth->orbit()->children)
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

TEST_CASE("location ids are unique and shared across every kind")
{
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    // Facility ids used to start at 1 and collide with body ids. They now draw from
    // the same sequence as every other location. Note they are NOT above all body
    // ids: the orbit and surface regions were appended after them, which is fine --
    // the invariant is uniqueness, not ordering.
    std::set<int> seen;
    int facilityCount = 0;
    int regionCount = 0;
    int highest = -1;

    for (auto &loc : game->allLocations())
    {
        if (!loc)
        {
            continue; // stored by id, so a slot may be empty
        }
        CHECK(seen.insert(loc->id).second); // unique across every location
        if (loc->id > highest)
        {
            highest = loc->id;
        }
        if (loc->isFacility())
        {
            ++facilityCount;
        }
        if (loc->type == LOCATION_TYPE_ORBIT || loc->type == LOCATION_TYPE_SURFACE)
        {
            ++regionCount;
        }
    }

    CHECK(facilityCount > 0);
    CHECK(regionCount > 0);
    CHECK(game->location_max_id == highest);
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

    // A surface facility sits in the body's surface region, which is at the body --
    // so only the small sibling offset separates it, much less than the orbit standoff.
    ResourceFacility *surface = game->resourceFacilityAt(earth);
    REQUIRE(surface != nullptr);
    const float groundOffset = distanceBetween(surface, earth);
    CHECK(groundOffset < standoff);
    CHECK(groundOffset < 1.0f);
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
        if (loc && loc->isFacility())
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
    CHECK(second->body() == earth);
    CHECK(second->primary == first->primary); // siblings in the same orbit region
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

TEST_CASE("an autopilot shuttle advances past its first dock")
{
    // The silent-failure regression. atEndpoint() compares the craft's location with
    // its endpoint's; if either side starts naming a region or a facility while the
    // other names a body, the comparison is never true, onDocked never advances the
    // endpoint, and the shuttle sits at its first dock forever with no error anywhere.
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *earth = game->locationByID(EARTH_ID);
    REQUIRE(earth != nullptr);
    Orbital *orbital = game->orbitalAt(earth);
    REQUIRE(orbital != nullptr);
    REQUIRE(game->resourceFacilityAt(earth) != nullptr); // both ends of the run exist

    Shuttle *s = game->createShuttle(earth);
    REQUIRE(s != nullptr);
    game->setDefaultRoute(s, orbital);
    s->setPodType(0, PT_SUPPLY);
    s->drive = true;
    s->fuel = 250;

    REQUIRE(s->engageAutopilot());
    REQUIRE(s->autopilot->state == AS_ON);

    const uint8_t startIndex = s->destination_index;
    ResourceFacility *station = game->resourceFacilityAt(earth);

    // Run the clock and watch BOTH ends. Advancing once is not enough: a shuttle that
    // reaches the surface station and then stalls there still advances its index on the
    // way in, so checking only that hid a real stall -- launching from a surface dock
    // left the craft in the surface region with nothing to make it climb.
    bool advanced = false;
    bool dockedAtStation = false;
    bool dockedAtOrbital = false;
    bool returnedToOrbital = false; // docked at the orbital AFTER visiting the surface

    for (int tick = 0; tick < 6000; ++tick)
    {
        game->update(0.05f);

        if (s->destination_index != startIndex)
        {
            advanced = true;
        }
        if (s->location == static_cast<Location *>(station))
        {
            dockedAtStation = true;
        }
        if (s->location == static_cast<Location *>(orbital))
        {
            dockedAtOrbital = true;
            if (dockedAtStation)
            {
                returnedToOrbital = true;
                break; // a full round trip, both ends served
            }
        }
    }

    CHECK_MESSAGE(advanced, "autopilot never advanced past its first endpoint");
    CHECK_MESSAGE(dockedAtStation, "autopilot never reached the surface station");
    CHECK_MESSAGE(dockedAtOrbital, "autopilot never reached the orbital");
    CHECK_MESSAGE(returnedToOrbital, "autopilot completed one leg but not a full cycle");
    CHECK(s->autopilot->state == AS_ON); // and did not disable itself on the way
}

TEST_CASE("a craft's location matches what it is doing")
{
    // The rule: docked states put the craft AT the facility, orbit and surface states
    // at the corresponding region. Driven by a real autopilot cycle rather than by
    // poking states, so every transition -- dock, work, undock, ascend, descend -- is
    // exercised the way the game exercises it.
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *earth = game->locationByID(EARTH_ID);
    REQUIRE(earth != nullptr);
    REQUIRE(earth->orbit() != nullptr);
    REQUIRE(earth->surface() != nullptr);

    Orbital *orbital = game->orbitalAt(earth);
    REQUIRE(orbital != nullptr);

    Shuttle *s = game->createShuttle(earth);
    REQUIRE(s != nullptr);
    game->setDefaultRoute(s, orbital);
    s->setPodType(0, PT_SUPPLY);
    s->drive = true;
    s->fuel = 250;
    REQUIRE(s->engageAutopilot());

    int violations = 0;
    int sawDocked = 0;
    int sawRegion = 0;

    for (int tick = 0; tick < 4000; ++tick)
    {
        game->update(0.05f);

        REQUIRE(s->location != nullptr);
        if (s->body() != earth)
        {
            ++violations; // a local run should never leave the body
            break;
        }

        // Counted by where it IS, not by state: with both endpoints docked the craft
        // passes through the regions inside a single tick, so a state sample would
        // miss them even though the regions are genuinely occupied.
        if (s->location == earth->orbit() || s->location == earth->surface())
        {
            ++sawRegion;
        }
        if (s->location->isFacility())
        {
            ++sawDocked;
        }

        // `docked()` and `inOrbit()` are now DEFINED as location->isFacility() and
        // location->inOrbit(), so cross-checking them against the state is a tautology
        // and no longer worth asserting -- that is the point of the collapse. What is
        // still worth asserting is that the craft only ever occupies a place a craft
        // can be: a facility, or one of the two regions. Never the bare body.
        const bool somewhereReal = s->location->isFacility() ||
                                   s->location == earth->orbit() ||
                                   s->location == earth->surface();
        if (!somewhereReal)
        {
            ++violations;
        }

        // A craft mid-manoeuvre is never docked: launching steps out of the facility
        // before the timer runs, and docking steps in only when it expires.
        if (s->moving() && s->docked())
        {
            ++violations;
        }
    }

    CHECK(violations == 0);
    CHECK(sawDocked > 0); // the cycle really did dock
    CHECK(sawRegion > 0); // and really did occupy a region undocked
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
    Shuttle *s = game->createShuttle(orbital);
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
    CHECK(game->createShuttle(orbital) == nullptr);
}

TEST_CASE("a craft is always somewhere")
{
    // Step 8 of docs/plans/orbit_as_location.md. A null craft location used to mean
    // "in space", which collided with location id 0 -- a real location, Sol space.
    // Null is now unrepresentable: the factories refuse it and the loader treats an
    // unresolvable id as an error rather than quietly producing a craft nowhere.
    const char *FL_SAVE_PATH = "./test_craft_location.db";

    Game *game = loadGame();
    REQUIRE(game != nullptr);

    CHECK(game->createShuttle(nullptr) == nullptr);
    CHECK(game->createIOS(static_cast<Location *>(nullptr)) == nullptr);

    // id 0 is Sol space: the "nowhere in particular" location, and a real one
    Location *space = game->locationByID(0);
    REQUIRE(space != nullptr);
    CHECK(space->type == LOCATION_TYPE_SPACE);

    Location *earth = game->locationByID(EARTH_ID);
    REQUIRE(earth != nullptr);
    Orbital *orbital = game->orbitalAt(earth);
    REQUIRE(orbital != nullptr);

    // one docked, one adrift -- the two shapes that have to survive a round trip
    Shuttle *docked = game->createShuttle(orbital);
    REQUIRE(docked != nullptr);
    IOS *adrift = game->createIOS(space);
    REQUIRE(adrift != nullptr);
    adrift->assignState(CS_TRANSIT, 0.0f, 0.0f);

    const int dockedId = docked->id;
    const int adriftId = adrift->id;

    SaveGame saver;
    REQUIRE(saver.save(FL_SAVE_PATH) == 0);

    Game *loaded = Game::createCurrent();
    Loader reloader(FL_SAVE_PATH);
    REQUIRE(reloader.isValid());
    REQUIRE(loaded->initialise(&reloader));

    int checked = 0;
    for (auto &shuttle : loaded->allShuttles())
    {
        REQUIRE(shuttle->location != nullptr);
        if (shuttle->id == dockedId)
        {
            CHECK(shuttle->location->type == LOCATION_TYPE_ORBITAL);
            CHECK(shuttle->location->body()->id == EARTH_ID);
            ++checked;
        }
    }
    for (auto &craft : loaded->allIOS())
    {
        REQUIRE(craft->location != nullptr);
        if (craft->id == adriftId)
        {
            // saved as id 0 and reloaded as the location, not as a null pointer
            CHECK(craft->location->id == 0);
            CHECK(craft->location->type == LOCATION_TYPE_SPACE);
            ++checked;
        }
    }
    CHECK(checked == 2);

    std::remove(FL_SAVE_PATH);
}

TEST_CASE("a surface facility mines the body it sits on")
{
    // Silent regression from facility parenting: `primary` is the surface REGION, and
    // resource availability is loaded onto bodies only, so reading availability off the
    // parent collected nothing at all while reporting no error anywhere.
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *earth = game->locationByID(EARTH_ID);
    REQUIRE(earth != nullptr);
    ResourceFacility *station = game->resourceFacilityAt(earth);
    REQUIRE(station != nullptr);

    // the facility hangs off the region; the region has no resources of its own
    REQUIRE(station->primary != nullptr);
    CHECK(station->primary->type == LOCATION_TYPE_SURFACE);
    CHECK(station->body() == earth);

    // find a resource Earth actually has
    int available = -1;
    for (int idx = 0; idx < ResourceType::Count; ++idx)
    {
        if (earth->resources.availability[idx])
        {
            available = idx;
            break;
        }
    }
    REQUIRE(available >= 0);
    CHECK(station->primary->resources.availability[available] == 0); // the region has none

    station->num_derricks = 1;
    station->damage = 0;
    const int before = station->stores.resources[available];

    station->update();

    CHECK(station->stores.resources[available] == before + 1);
}

TEST_CASE("verbs refuse exactly what their canX() refuses")
{
    // The point of canX(): one answer, asked by the UI to decide what to draw and by the
    // verb to decide whether to act. If a verb accepts something its guard refuses, the
    // UI and the game disagree, and the autopilot -- which calls verbs directly, never
    // the UI -- gets the looser behaviour.
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *earth = game->locationByID(EARTH_ID);
    REQUIRE(earth != nullptr);
    Orbital *orbital = game->orbitalAt(earth);
    ResourceFacility *station = game->resourceFacilityAt(earth);
    REQUIRE(orbital != nullptr);
    REQUIRE(station != nullptr);

    Shuttle *s = earth->shuttle ? earth->shuttle : game->createShuttle(earth);
    REQUIRE(s != nullptr);
    s->drive = true;

    SUBCASE("a docked craft cannot engage its drive")
    {
        IOS *ios = game->createIOS(static_cast<Location *>(orbital));
        REQUIRE(ios != nullptr);
        ios->drive = true;
        ios->location = orbital;
        ios->assignState(CS_IDLE, 0.0f, 0.0f);
        REQUIRE(ios->docked());

        // Flying off while still made fast to a station should not be possible.
        CHECK_FALSE(bool(ios->canEngageDrive()));
        ios->engageDrive();
        CHECK(ios->docked()); // and the verb must not have moved it either
    }

    SUBCASE("ascending is never done while still docked")
    {
        s->location = station;
        s->assignState(CS_IDLE, 0.0f, 0.0f);
        REQUIRE(s->docked());

        s->ascend();
        // Either the guard refuses a docked start, or the verb steps out of the facility
        // first. What must not happen is climbing while still inside the station.
        const bool climbingWhileDocked = s->moving() && s->docked();
        CHECK_FALSE(climbingWhileDocked);
    }

    SUBCASE("launch obeys its own guard")
    {
        s->location = station;
        s->assignState(CS_IDLE, 0.0f, 0.0f);
        s->drive = false; // canLaunch() requires a drive

        REQUIRE_FALSE(bool(s->canLaunch()));
        s->launch();
        CHECK(s->docked()); // refused, so still at the station
    }
}
