// The computer opponent: FactionController (drone pooling, warship spawn, reinforcement,
// tasking) and Craft::runAI (boarding, launching after a refit).
//
// The controller is driven directly with update() where a case is about one decision,
// and through the fixed-tick simulation where it is about craft behaviour over time.

#include "doctest.h"
#include "../include/loaders/loader.h"
#include "../include/state/game.h"
#include "../include/state/faction_controller.h"
#include "../include/state/system.h"
#include "../include/state/location.h"
#include "../include/state/facility.h"
#include "../include/state/orbital.h"
#include "../include/state/ios.h"
#include "../include/state/event_sink.h"
#include "sim_support.h"

#include <vector>

namespace
{
    const char *FC_DB_PATH = "./resources/initial.db";
    const int EARTH_ID = 4;

    class EarthCaptureSink : public EventSink
    {
    public:
        std::vector<Orbital *> captured;
        void onEarthOrbitalCaptured(Orbital *orbital) override { captured.push_back(orbital); }
    };

    struct AIFixture
    {
        Game *game{nullptr};
        FactionController *ai{nullptr};
        Orbital *home{nullptr}; // a methanoid orbital in the same system as Earth
        Location *earth{nullptr};

        AIFixture()
        {
            game = Game::createCurrent();
            Loader loader(FC_DB_PATH);
            if (!loader.isValid() || !game->initialise(&loader))
            {
                game = nullptr;
                return;
            }
            earth = game->locationByID(EARTH_ID);
            if (FACTION_METHANOID < static_cast<int>(game->factionControllers.size()))
            {
                ai = game->factionControllers[FACTION_METHANOID].get();
            }
            if (ai && earth)
            {
                auto orbitals = ai->systemOrbitals(earth->system->id);
                if (!orbitals.empty())
                {
                    home = orbitals.front();
                }
            }
            // Start from empty stores so only what a case adds is excess.
            for (Orbital *o : game->allOrbitals())
            {
                o->stores.items[ItemType::Ios_Drone] = 0;
            }
        }

        bool valid() const { return game && ai && home && earth; }

        void setHostile()
        {
            // Both sides, as buildTestData does. See the skipped case below for why the
            // player faction has to be hostile too.
            game->setFactionHostility(FACTION_TERRAN, true);
            game->setFactionHostility(FACTION_METHANOID, true);
        }

        std::vector<Craft *> warships() const
        {
            std::vector<Craft *> result;
            for (auto &c : game->allIOS())
            {
                if (c->faction_id == FACTION_METHANOID && c->isWarship())
                {
                    result.push_back(c.get());
                }
            }
            return result;
        }

        Location *homeOrbit() const { return home->body()->orbit(); }

        // The garrison level is private to the controller; observe it by letting one step
        // trim an over-full store, then remove the warship that step produced.
        int discoverGarrison()
        {
            home->stores.items[ItemType::Ios_Drone] = 1000;
            ai->update();
            const int garrison = home->stores.items[ItemType::Ios_Drone];
            for (Craft *c : warships())
            {
                game->destroyCraft(c);
            }
            return garrison;
        }
    };

} // namespace

TEST_CASE("the opponent does nothing until it is hostile")
{
    AIFixture f;
    REQUIRE(f.valid());

    f.home->stores.items[ItemType::Ios_Drone] = 1000;
    f.ai->update();

    CHECK(f.warships().empty());
    CHECK(f.home->stores.items[ItemType::Ios_Drone] == 1000);
}

TEST_CASE("drones above the garrison spawn one warship carrying them")
{
    AIFixture f;
    REQUIRE(f.valid());
    f.setHostile();

    const int garrison = f.discoverGarrison();
    REQUIRE(garrison > 0);
    REQUIRE(f.warships().empty());

    f.home->stores.items[ItemType::Ios_Drone] = garrison + 120;
    f.ai->update();

    CHECK(f.home->stores.items[ItemType::Ios_Drone] == garrison);
    auto ships = f.warships();
    REQUIRE(ships.size() == 1);
    CHECK(f.game->droneCountForCraft(ships[0]) == 120); // nothing created or lost
    CHECK(ships[0]->body() == f.home->body());
    CHECK(ships[0]->inOrbit());
    CHECK_FALSE(ships[0]->docked());
}

TEST_CASE("further excess reinforces the idle warship rather than spawning another")
{
    AIFixture f;
    REQUIRE(f.valid());
    f.setHostile();

    const int garrison = f.discoverGarrison();
    f.home->stores.items[ItemType::Ios_Drone] = garrison + 100;
    f.ai->update();
    REQUIRE(f.warships().size() == 1);
    Craft *ship = f.warships()[0];
    const int before = f.game->droneCountForCraft(ship);
    REQUIRE(before == 100);

    f.home->stores.items[ItemType::Ios_Drone] = garrison + 30;
    f.ai->update();

    CHECK(f.warships().size() == 1);
    CHECK(f.game->droneCountForCraft(ship) == before + 30);
    CHECK(f.home->stores.items[ItemType::Ios_Drone] == garrison);
}

TEST_CASE("a warship in transit is not reinforced; the excess starts a new one")
{
    AIFixture f;
    REQUIRE(f.valid());
    f.setHostile();

    const int garrison = f.discoverGarrison();
    Craft *away = f.game->spawnWarship(FACTION_METHANOID, f.homeOrbit(), 1, 50);
    REQUIRE(away != nullptr);
    away->setDestination(away->destination_index, f.earth);
    away->engageDrive();
    REQUIRE(away->inTransit());

    f.home->stores.items[ItemType::Ios_Drone] = garrison + 40;
    f.ai->update();

    CHECK(f.game->droneCountForCraft(away) == 50);
    CHECK(f.warships().size() == 2);
}

TEST_CASE("a full idle warship is sent to the nearest operational hostile orbital")
{
    AIFixture f;
    REQUIRE(f.valid());
    f.setHostile();

    Orbital *target = ensureOrbital(f.game, f.earth); // the player's, built in code
    REQUIRE(target != nullptr);
    REQUIRE(target->faction_id == FACTION_TERRAN);
    REQUIRE(target->system == f.home->system);

    Craft *ship = f.game->spawnWarship(FACTION_METHANOID, f.homeOrbit(), 1, MAX_DRONE_FLEET_SIZE);
    REQUIRE(ship != nullptr);

    SUBCASE("operational: it leaves for it")
    {
        f.ai->update();
        CHECK(ship->inTransit());
        CHECK(ship->currentDestination().location == target);
    }

    SUBCASE("still under construction: it stays")
    {
        target->operational = false;
        f.ai->update();
        CHECK_FALSE(ship->moving());
    }

    SUBCASE("destroyed: it stays")
    {
        f.game->destroyFacility(target);
        f.ai->update();
        CHECK_FALSE(ship->moving());
    }
}

TEST_CASE("a part-filled warship is not sent anywhere")
{
    AIFixture f;
    REQUIRE(f.valid());
    f.setHostile();
    REQUIRE(ensureOrbital(f.game, f.earth) != nullptr);

    Craft *ship = f.game->spawnWarship(FACTION_METHANOID, f.homeOrbit(), 1, MAX_DRONE_FLEET_SIZE - 1);
    REQUIRE(ship != nullptr);

    f.ai->update();
    CHECK_FALSE(ship->moving());
}

TEST_CASE("a warship in orbit at its own faction's station does not dock there")
{
    AIFixture f;
    REQUIRE(f.valid());
    f.setHostile();

    Craft *ship = f.game->spawnWarship(FACTION_METHANOID, f.homeOrbit(), 1, 50);
    REQUIRE(ship != nullptr);

    for (int step = 0; step < 64 * 3; ++step)
    {
        f.game->tick();
        REQUIRE_FALSE(ship->docked());
        REQUIRE(ship->currentState().state != CS_DOCKING);
    }
}

TEST_CASE("a warship docked at its own station launches once its refit is done")
{
    AIFixture f;
    REQUIRE(f.valid());

    Craft *ship = f.game->spawnWarship(FACTION_METHANOID, f.homeOrbit(), 1, 50);
    REQUIRE(ship != nullptr);
    ship->location = f.home; // docked
    ship->setTimedState(CS_WORKING, CSTD_BOARD_HOSTILE_ORBITAL);
    REQUIRE(ship->docked());

    runFor(f.game, CSTD_BOARD_HOSTILE_ORBITAL * 0.5);
    CHECK(ship->docked()); // still refitting

    runFor(f.game, CSTD_BOARD_HOSTILE_ORBITAL + 0.5);
    CHECK_FALSE(ship->docked());
    CHECK(ship->body() == f.home->body());
}

TEST_CASE("boarding the player's Earth orbital captures it and raises onEarthOrbitalCaptured")
{
    AIFixture f;
    REQUIRE(f.valid());
    f.setHostile();

    Orbital *earthOrbital = ensureOrbital(f.game, f.earth);
    REQUIRE(earthOrbital != nullptr);
    earthOrbital->stores.items[ItemType::Ios_Drone] = 0;

    EarthCaptureSink sink;
    f.game->addEventSink(&sink);

    // Part-filled, so the controller never re-tasks it during the case.
    Craft *ship = f.game->spawnWarship(FACTION_METHANOID, f.earth->orbit(), 1, 50);
    REQUIRE(ship != nullptr);

    for (int step = 0; step < 64 * 10 && earthOrbital->faction_id != FACTION_METHANOID; ++step)
    {
        f.game->tick();
    }

    CHECK(earthOrbital->faction_id == FACTION_METHANOID);
    REQUIRE(sink.captured.size() == 1);
    CHECK(sink.captured[0] == earthOrbital);

    // ...then refits and returns to orbit.
    runFor(f.game, CSTD_BOARD_HOSTILE_ORBITAL + 1.0);
    CHECK_FALSE(ship->docked());

    f.game->removeEventSink(&sink);
}

TEST_CASE("capturing an orbital away from Earth does not raise onEarthOrbitalCaptured")
{
    AIFixture f;
    REQUIRE(f.valid());

    EarthCaptureSink sink;
    f.game->addEventSink(&sink);

    f.game->onCaptureOrbital(f.home, FACTION_TERRAN);
    CHECK(f.home->faction_id == FACTION_TERRAN);
    CHECK(sink.captured.empty());

    f.game->removeEventSink(&sink);
}

TEST_CASE("capture works when only the methanoids have been declared hostile")
{
    // The in-game routes (six orbitals, the sympathiser's warning) declare faction 1
    // hostile. hostilesAt() checks the hostility of the orbital's OWNER, so the player
    // faction must become hostile too, or a warship docking at a player orbital is not
    // seen as boarding and nothing is captured.
    AIFixture f;
    REQUIRE(f.valid());
    f.game->setFactionHostility(FACTION_METHANOID, true);

    Orbital *earthOrbital = ensureOrbital(f.game, f.earth);
    REQUIRE(earthOrbital != nullptr);
    Craft *ship = f.game->spawnWarship(FACTION_METHANOID, f.earth->orbit(), 1, 50);
    REQUIRE(ship != nullptr);

    runFor(f.game, 10.0);
    CHECK(earthOrbital->faction_id == FACTION_METHANOID);
}

TEST_CASE("a newly spawned warship never carries more than a full fleet")
{
    // Tasking sends only warships at exactly MAX_DRONE_FLEET_SIZE, so one spawned above it
    // from a large surplus would never leave.
    AIFixture f;
    REQUIRE(f.valid());
    f.setHostile();

    f.home->stores.items[ItemType::Ios_Drone] = 1000;
    f.ai->update();

    for (Craft *c : f.warships())
    {
        CHECK(f.game->droneCountForCraft(c) <= MAX_DRONE_FLEET_SIZE);
    }
}

TEST_CASE("a warship goes on to a second target after its first capture")
{
    // Docking at an endpoint advances destination_index, so tasking must write the new
    // target into the CURRENT slot; otherwise engageDrive flies to the stale one -- the
    // warship's own spawn orbital -- and it never attacks again.
    AIFixture f;
    REQUIRE(f.valid());
    f.setHostile();

    Orbital *first = ensureOrbital(f.game, f.earth);
    Location *mars = f.game->locationByID(6);
    REQUIRE(mars != nullptr);
    Orbital *second = ensureOrbital(f.game, mars);
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    Craft *ship = f.game->spawnWarship(FACTION_METHANOID, f.homeOrbit(), 1, MAX_DRONE_FLEET_SIZE);
    REQUIRE(ship != nullptr);

    for (int step = 0; step < 64 * 600 &&
                       (first->faction_id != FACTION_METHANOID || second->faction_id != FACTION_METHANOID);
         ++step)
    {
        f.game->tick();
    }
    CHECK(first->faction_id == FACTION_METHANOID);
    CHECK(second->faction_id == FACTION_METHANOID);
}
