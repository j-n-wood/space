// ViewState separates two questions that used to share one field:
//
//   getCurrentPlace() -- exactly where the focus is (facility, region, or body)
//   getCurrentBody()  -- the body whose facilities the sidebar offers
//
// Both derive from the focus rather than being stored alongside it, so they cannot
// drift apart. These pin that.

#include "doctest.h"
#include "../include/loaders/loader.h"
#include "../include/pages/view_state.h"
#include "../include/state/game.h"
#include "../include/state/system.h"
#include "../include/state/location.h"
#include "../include/state/facility.h"
#include "../include/state/resourceFacility.h"
#include "../include/state/orbital.h"
#include "../include/state/shuttle.h"

namespace
{

const char *VS_DB_PATH = "./resources/initial.db";
const int EARTH_ID = 4;

Game *loadGame()
{
    Game *game = Game::createCurrent();
    Loader loader(VS_DB_PATH);
    if (!loader.isValid() || !game->initialise(&loader))
    {
        return nullptr;
    }
    return game;
}

} // namespace

TEST_CASE("a craft docked at a surface station reports that station")
{
    // The regression. setCraftFocus used to find the facility by trying orbitalAt()
    // first and falling back to resourceFacilityAt(). orbitalAt matches on body(), so a
    // craft docked on the ground at a body that ALSO has an orbital reported the
    // orbital. Earth is such a body, which is why it is the one used here.
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *earth = game->locationByID(EARTH_ID);
    REQUIRE(earth != nullptr);

    ResourceFacility *station = game->resourceFacilityAt(earth);
    Orbital *orbital = game->orbitalAt(earth);
    REQUIRE(station != nullptr);
    REQUIRE(orbital != nullptr); // both sides present -- the condition for the bug

    Shuttle *shuttle = earth->shuttle ? earth->shuttle : game->createShuttle(earth);
    REQUIRE(shuttle != nullptr);

    ViewState vs;

    SUBCASE("docked on the surface")
    {
        shuttle->location = station;
        shuttle->state = CS_SURFACE_DOCKED;
        vs.setCraftFocus(shuttle);

        CHECK(vs.getCurrentPlace() == static_cast<Location *>(station));
        CHECK(vs.getCurrentFacility() == static_cast<Facility *>(station));
        CHECK(vs.getCurrentBody() == earth); // sidebar still offers both sides
    }

    SUBCASE("docked at the orbital")
    {
        shuttle->location = orbital;
        shuttle->state = CS_ORBIT_DOCKED;
        vs.setCraftFocus(shuttle);

        CHECK(vs.getCurrentPlace() == static_cast<Location *>(orbital));
        CHECK(vs.getCurrentFacility() == static_cast<Facility *>(orbital));
        CHECK(vs.getCurrentBody() == earth);
    }

    SUBCASE("in orbit with no dock -- at a place, but not at a facility")
    {
        Location *orbitRegion = earth->orbit();
        REQUIRE(orbitRegion != nullptr);
        shuttle->location = orbitRegion;
        shuttle->state = CS_ORBIT;
        vs.setCraftFocus(shuttle);

        CHECK(vs.getCurrentPlace() == orbitRegion);
        CHECK(vs.getCurrentFacility() == nullptr); // never guessed
        CHECK(vs.getCurrentBody() == earth);
    }
}

TEST_CASE("the sidebar body is always a celestial body")
{
    // Everything the sidebar reads is body-scoped: orbitalAt / resourceFacilityAt
    // normalise internally, but location->shuttle and location->resources do not. So
    // getCurrentBody() must never hand back a region or a facility.
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *earth = game->locationByID(EARTH_ID);
    REQUIRE(earth != nullptr);
    Orbital *orbital = game->orbitalAt(earth);
    REQUIRE(orbital != nullptr);
    Shuttle *shuttle = earth->shuttle ? earth->shuttle : game->createShuttle(earth);
    REQUIRE(shuttle != nullptr);

    ViewState vs;

    Location *places[] = {earth, earth->orbit(), earth->surface(), orbital};
    for (Location *place : places)
    {
        REQUIRE(place != nullptr);
        shuttle->location = place;
        vs.setCraftFocus(shuttle);

        Location *body = vs.getCurrentBody();
        REQUIRE(body != nullptr);
        CHECK(locationIsBody(body->type));
        CHECK(body == earth);
        // the property the sidebar actually depends on
        CHECK(body->shuttle == shuttle);
    }

    // facility focus and location focus land on the same body
    vs.setFacilityFocus(orbital);
    CHECK(vs.getCurrentBody() == earth);
    CHECK(vs.getCurrentPlace() == static_cast<Location *>(orbital));

    vs.setLocationFocus(earth);
    CHECK(vs.getCurrentBody() == earth);
    CHECK(vs.getCurrentPlace() == earth);
    CHECK(vs.getCurrentFacility() == nullptr);
}

TEST_CASE("the focus follows a craft with no resync")
{
    // The property that let the overlay's per-frame re-pin go: place is read THROUGH
    // the craft, so moving the craft moves the focus with no ViewState call between.
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *earth = game->locationByID(EARTH_ID);
    REQUIRE(earth != nullptr);
    Orbital *orbital = game->orbitalAt(earth);
    ResourceFacility *station = game->resourceFacilityAt(earth);
    REQUIRE(orbital != nullptr);
    REQUIRE(station != nullptr);

    Shuttle *shuttle = earth->shuttle ? earth->shuttle : game->createShuttle(earth);
    REQUIRE(shuttle != nullptr);

    ViewState vs;
    shuttle->location = orbital;
    vs.setCraftFocus(shuttle);
    REQUIRE(vs.getCurrentPlace() == static_cast<Location *>(orbital));

    // undock -- straight to the region the facility sits in
    shuttle->state = CS_ORBIT_DOCKED;
    shuttle->launch();
    CHECK(vs.getCurrentPlace() == earth->orbit());
    CHECK(vs.getCurrentBody() == earth);

    // descend
    shuttle->enterRegion(false);
    CHECK(vs.getCurrentPlace() == earth->surface());
    CHECK(vs.getCurrentBody() == earth);

    // dock at the station
    shuttle->location = station;
    CHECK(vs.getCurrentPlace() == static_cast<Location *>(station));
    CHECK(vs.getCurrentFacility() == static_cast<Facility *>(station));
}

TEST_CASE("dropping craft focus stays where the craft was")
{
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    Location *earth = game->locationByID(EARTH_ID);
    REQUIRE(earth != nullptr);
    Orbital *orbital = game->orbitalAt(earth);
    REQUIRE(orbital != nullptr);
    Shuttle *shuttle = earth->shuttle ? earth->shuttle : game->createShuttle(earth);
    REQUIRE(shuttle != nullptr);

    ViewState vs;
    vs.setLocationFocus(earth); // somewhere else first, so a fallback would show
    shuttle->location = orbital;
    vs.setCraftFocus(shuttle);
    REQUIRE(vs.getCurrentPlace() == static_cast<Location *>(orbital));

    // the sidebar clears craft focus on every standard-button press
    vs.setCurrentCraft(nullptr);
    CHECK(vs.getCurrentCraft() == nullptr);
    CHECK(vs.getCurrentPlace() == static_cast<Location *>(orbital)); // not back to Earth
    CHECK(vs.getCurrentBody() == earth);

    // and the craft moving no longer drags the view along
    shuttle->location = earth->surface();
    CHECK(vs.getCurrentPlace() == static_cast<Location *>(orbital));
}

TEST_CASE("focused on nothing is representable")
{
    // Master control: no place, no body, no crash. The sidebar renders empty.
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    ViewState vs;
    CHECK(vs.getCurrentPlace() == nullptr);
    CHECK(vs.getCurrentBody() == nullptr);
    CHECK(vs.getCurrentFacility() == nullptr);
    CHECK(vs.getCurrentCraft() == nullptr);
    CHECK(vs.getCurrentSystem() == nullptr);

    // browsing a system with nothing focused still reports that system
    System *sol = game->allSystems()[1].get();
    REQUIRE(sol != nullptr);
    vs.setCurrentSystem(sol);
    CHECK(vs.getCurrentSystem() == sol);
    CHECK(vs.getCurrentPlace() == nullptr);

    // and a focus supplies its own system
    Location *earth = game->locationByID(EARTH_ID);
    REQUIRE(earth != nullptr);
    vs.setLocationFocus(earth);
    CHECK(vs.getCurrentSystem() == earth->system);
}
