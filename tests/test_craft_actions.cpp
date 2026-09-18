// Craft actions: the canX() guards and the verbs that delegate to them.
//
// The contract under test is that a guard and its verb are the same question asked once.
// The UI calls canX() to decide what to draw; the autopilot calls the verb directly and
// never sees the UI. If the two disagree, the game does something the player was told was
// impossible -- or refuses something the player was offered.

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
#include "../include/state/autopilot.h"
#include "../include/state/craft_action.h"

namespace
{

const char *CA_DB_PATH = "./resources/initial.db";
const int EARTH_ID = 4;

Game *loadGame()
{
    Game *game = Game::createCurrent();
    Loader loader(CA_DB_PATH);
    if (!loader.isValid() || !game->initialise(&loader))
    {
        return nullptr;
    }
    return game;
}

// Earth carries both an orbital and a surface facility, which is what makes it the only
// body where every guard can be exercised without building anything first.
struct Fixture
{
    Game *game;
    Location *earth;
    Orbital *orbital;
    ResourceFacility *station;

    Fixture() : game(loadGame()), earth(nullptr), orbital(nullptr), station(nullptr)
    {
        if (!game)
        {
            return;
        }
        earth = game->locationByID(EARTH_ID);
        orbital = game->orbitalAt(earth);
        station = game->resourceFacilityAt(earth);
    }

    bool valid() const
    {
        return game && earth && orbital && station && earth->orbit() && earth->surface();
    }

    // A shuttle parked exactly where the case needs it, at rest and fully fitted.
    Shuttle *shuttleAt(Location *where) const
    {
        Shuttle *s = earth->shuttle ? earth->shuttle : game->createShuttle(earth);
        s->drive = true;
        s->fuel = 250;
        s->location = where;
        s->assignState(CS_IDLE, 0.0f, 0.0f);
        return s;
    }
};

} // namespace

TEST_CASE("an unasked action reads as refused")
{
    // CAC_UNKNOWN is 0 so a value-initialised or memset result fails safe -- it is never
    // returned deliberately, and must never test as success.
    CraftActionResult r;
    CHECK_FALSE(bool(r));
    CHECK(r.code() == CAC_UNKNOWN);
    CHECK(bool(CraftActionResult{CAC_OK}));
}

TEST_CASE("capability refusals do not depend on position or fitment")
{
    Fixture f;
    REQUIRE(f.valid());

    SUBCASE("a shuttle is not rated for interplanetary transit")
    {
        Shuttle *s = f.shuttleAt(f.earth->orbit());
        s->setDestination(0, f.game->locationByID(6)); // Mars
        CHECK(s->canEngageDrive().code() == CAC_NOT_CAPABLE);

        s->engageDrive();
        CHECK_FALSE(s->inTransit()); // and the verb agrees
    }

    SUBCASE("an IOS cannot descend, and so cannot ascend either")
    {
        IOS *ios = f.game->createIOS(f.earth->orbit());
        REQUIRE(ios != nullptr);
        ios->drive = true;
        ios->location = f.earth->orbit();
        ios->assignState(CS_IDLE, 0.0f, 0.0f);

        CHECK(ios->canDescend().code() == CAC_NOT_CAPABLE);
        ios->descend();
        CHECK(ios->location == f.earth->orbit());

        ios->location = f.earth->surface();
        CHECK(ios->canAscend().code() == CAC_NOT_CAPABLE);
    }
}

TEST_CASE("guards answer the most specific tier first")
{
    // Capability, then fitment, then situation. A craft that fails several must report
    // the most fundamental, or the player fits a drive and is refused all over again for
    // a reason that was true the whole time.
    Fixture f;
    REQUIRE(f.valid());

    SUBCASE("capability outranks fitment")
    {
        // An IOS in the wrong place with no drive still cannot ever descend.
        IOS *ios = f.game->createIOS(f.earth->orbit());
        REQUIRE(ios != nullptr);
        ios->drive = false;
        ios->location = f.earth->surface();
        ios->assignState(CS_IDLE, 0.0f, 0.0f);
        CHECK(ios->canDescend().code() == CAC_NOT_CAPABLE);
    }

    SUBCASE("fitment outranks situation")
    {
        // On the surface, so the position is wrong for descending AND the drive is
        // missing. The drive is the answer.
        Shuttle *s = f.shuttleAt(f.earth->surface());
        s->drive = false;
        CHECK(s->canDescend().code() == CAC_NO_DRIVE);
    }

    SUBCASE("situation is reported once capability and fitment are satisfied")
    {
        Shuttle *s = f.shuttleAt(f.earth->surface());
        CHECK(s->canDescend().code() == CAC_WRONG_STATE); // on the ground already
        CHECK(s->canAscend().code() == CAC_OK);
    }
}

TEST_CASE("docking reports why it is refused")
{
    Fixture f;
    REQUIRE(f.valid());

    SUBCASE("nothing to dock with")
    {
        Location *luna = f.game->locationByID(5);
        REQUIRE(luna != nullptr);
        REQUIRE(f.game->orbitalAt(luna) == nullptr); // Luna has a base, not an orbital
        Shuttle *s = f.shuttleAt(luna->orbit());
        CHECK(s->canDock().code() == CAC_NO_ORBITAL);
    }

    SUBCASE("the station is still under construction")
    {
        Shuttle *s = f.shuttleAt(f.earth->orbit());
        f.orbital->operational = false;
        CHECK(s->canDock().code() == CAC_ORBITAL_INCOMPLETE);

        s->dock();
        CHECK(s->location == f.earth->orbit()); // verb refused too
    }

    SUBCASE("the station is hostile and defended")
    {
        Shuttle *s = f.shuttleAt(f.earth->orbit());
        f.orbital->operational = true;
        f.orbital->faction_id = 1;
        f.game->setFactionHostility(1, true);
        f.orbital->stores.items[ItemType::Ios_Drone] = 3;

        CHECK(s->canDock().code() == CAC_DEFENDED);

        // Clearing the drones is what opens it, not the hostility itself.
        f.orbital->stores.items[ItemType::Ios_Drone] = 0;
        CHECK(s->canDock().code() == CAC_OK);
    }

    SUBCASE("already docked")
    {
        Shuttle *s = f.shuttleAt(f.orbital);
        REQUIRE(s->docked());
        CHECK_FALSE(bool(s->canDock())); // the Dock and Undock controls are exclusive
    }
}

TEST_CASE("engaging the autopilot reports why it is refused")
{
    Fixture f;
    REQUIRE(f.valid());

    SUBCASE("no supply pod fitted")
    {
        Shuttle *s = f.shuttleAt(f.orbital);
        f.game->setDefaultRoute(s, f.orbital);
        s->setPodType(0, PT_EMPTY);
        CHECK(s->canEngageAutopilot().code() == CAC_NO_SUPPLY_POD);
        CHECK_FALSE(bool(s->engageAutopilot()));
        CHECK(s->autopilot->state == AS_OFF);
    }

    SUBCASE("a route this hull cannot fly")
    {
        // A shuttle is not interplanetary, so an endpoint at another body is unreachable.
        // Caught at engagement rather than every tick, which is what stops the autopilot
        // asking for a transit it can never have.
        Shuttle *s = f.shuttleAt(f.orbital);
        s->setPodType(0, PT_SUPPLY);
        f.game->setDefaultRoute(s, f.orbital);
        s->setDestination(0, f.game->locationByID(6)); // Mars
        CHECK(s->canEngageAutopilot().code() == CAC_ROUTE_UNREACHABLE);
        CHECK_FALSE(bool(s->engageAutopilot()));
    }

    SUBCASE("a local route is accepted")
    {
        Shuttle *s = f.shuttleAt(f.orbital);
        s->setPodType(0, PT_SUPPLY);
        f.game->setDefaultRoute(s, f.orbital);
        CHECK(s->canEngageAutopilot().code() == CAC_OK);
        CHECK(bool(s->engageAutopilot()));
        CHECK(s->autopilot->state == AS_ON);
    }
}

TEST_CASE("every verb refuses exactly what its guard refuses")
{
    // The equivalence itself, swept over the places a craft can be. A verb that acts when
    // its guard says no is the failure this whole layer exists to prevent.
    Fixture f;
    REQUIRE(f.valid());

    Location *places[] = {
        f.earth->orbit(),
        f.earth->surface(),
        static_cast<Location *>(f.orbital),
        static_cast<Location *>(f.station),
    };

    for (Location *where : places)
    {
        REQUIRE(where != nullptr);

        for (int withDrive = 0; withDrive < 2; ++withDrive)
        {
            Shuttle *s = f.shuttleAt(where);
            s->drive = (withDrive != 0);

            const bool mayLaunch = bool(s->canLaunch());
            s->launch();
            CHECK(mayLaunch == s->isLaunching());

            s = f.shuttleAt(where);
            s->drive = (withDrive != 0);
            const bool mayAscend = bool(s->canAscend());
            s->ascend();
            CHECK(mayAscend == (s->currentState().state == CS_ASCENDING));

            s = f.shuttleAt(where);
            s->drive = (withDrive != 0);
            const bool mayDescend = bool(s->canDescend());
            s->descend();
            CHECK(mayDescend == (s->currentState().state == CS_DESCENDING));

            s = f.shuttleAt(where);
            s->drive = (withDrive != 0);
            const bool mayDock = bool(s->canDock());
            s->dock();
            CHECK(mayDock == s->isDocking());
        }
    }
}

TEST_CASE("a blocked launch is retried, not lost")
{
    // Why the departure decision lives in Autopilot::update rather than in a callback:
    // a one-shot onDockWorkComplete() fires once, so a launch refused at that instant
    // stranded the craft permanently. Being a condition re-tested every tick means the
    // craft leaves as soon as it can.
    Fixture f;
    REQUIRE(f.valid());

    Shuttle *s = f.shuttleAt(f.orbital);
    s->setPodType(0, PT_SUPPLY);
    f.game->setDefaultRoute(s, f.orbital);
    REQUIRE(bool(s->engageAutopilot()));

    // Lose the drive while docked: every manoeuvre is now refused.
    s->drive = false;
    for (int tick = 0; tick < 200; ++tick)
    {
        f.game->update(0.05f);
    }
    CHECK(s->docked()); // stuck, correctly -- it cannot fly

    // Fit one. Nothing re-notifies the autopilot; it simply asks again next tick.
    s->drive = true;
    bool departed = false;
    for (int tick = 0; tick < 200 && !departed; ++tick)
    {
        f.game->update(0.05f);
        if (!s->docked())
        {
            departed = true;
        }
    }
    CHECK_MESSAGE(departed, "autopilot never retried the launch once the drive was fitted");
}

TEST_CASE("engaging while docked loads the pods")
{
    // Engaging at a station has no docking transition to hang loading off, so
    // engageAutopilot has to do it -- and must set AS_ON first, or Autopilot::onDocked
    // returns immediately and the craft departs empty.
    Fixture f;
    REQUIRE(f.valid());

    f.orbital->stores.resources[ResourceType::Iron] = 500;

    Shuttle *s = f.shuttleAt(f.orbital);
    s->setPodType(0, PT_SUPPLY);
    f.game->setDefaultRoute(s, f.orbital); // [0] = the surface station, [1] = the orbital
    s->destination_index = 1;              // so the craft is AT its current endpoint
    s->autopilot->flow[ResourceType::Iron] = RF_LOAD_AT_DEST;
    REQUIRE(s->pods[0].amount == 0);

    REQUIRE(bool(s->engageAutopilot()));
    CHECK(s->autopilot->state == AS_ON);
    CHECK_MESSAGE(s->pods[0].amount > 0, "engaging at a station left the pod empty");
}

TEST_CASE("engaging while docked somewhere off-route loads nothing")
{
    // The atEndpoint() guard: a craft docked at a station that is not its current
    // endpoint has not arrived anywhere, so there is nothing to load. Loading anyway
    // would draw from the wrong facility's stores entirely.
    Fixture f;
    REQUIRE(f.valid());

    f.station->stores.resources[ResourceType::Iron] = 500;

    Shuttle *s = f.shuttleAt(f.orbital);
    s->setPodType(0, PT_SUPPLY);
    f.game->setDefaultRoute(s, f.orbital);
    s->destination_index = 0; // the SURFACE station, while docked at the orbital
    s->autopilot->flow[ResourceType::Iron] = RF_LOAD_AT_SOURCE;

    REQUIRE(bool(s->engageAutopilot()));
    CHECK_MESSAGE(s->pods[0].amount == 0, "loaded from a station the craft was not at");
}
