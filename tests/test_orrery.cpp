// The orrery's per-location render table.
//
// The table exists so the draw, hover and click paths read one set of screen coordinates
// instead of each recomputing them, and so a paused orrery does no position work at all.
// Both properties are testable without a window: rebuilding and hit-testing want only a
// System, a scale and a centre.

#include "doctest.h"
#include "../include/orrery.h"
#include "../include/loaders/loader.h"
#include "../include/state/game.h"
#include "../include/state/system.h"
#include "../include/state/location.h"
#include "../include/state/facility.h"

#include <cmath>

namespace
{

    const char *OR_DB_PATH = "./resources/initial.db";
    const int BELT_ID = 9; // Sol's asteroid belt, the only LOCATION_TYPE_ASTEROID_BELT body

    const int EARTH_ID = 4;
    const int JUPITER_ID = 10;

    Game *loadGame()
    {
        Game *game = Game::createCurrent();
        Loader loader(OR_DB_PATH);
        if (!loader.isValid() || !game->initialise(&loader))
        {
            return nullptr;
        }
        return game;
    }

    // Sol, reached through a body in it rather than by index -- allSystems()[0] is interstellar
    // space, and the orrery only ever cares which system a location belongs to.
    System *solSystem(Game *game)
    {
        Location *earth = game->locationByID(EARTH_ID);
        return earth ? earth->system : nullptr;
    }

    // An orrery on the loaded system, focused on the origin so focusOffset() is just the centre
    // -- which makes the belt band's centre a known point rather than one the test has to derive.
    OrreryPtr solOrrery(Game *game, float scale = 1.0f)
    {
        OrreryPtr orrery = createOrrery((Vector2){640.0f, 400.0f}, scale);
        orrery->setSystem(solSystem(game));
        orrery->focus = {0.0f, 0.0f};
        orrery->focus_location = nullptr;
        orrery->update();
        return orrery;
    }

    const LocationRender *rowFor(const Orrery *orrery, const Location *loc)
    {
        for (const LocationRender &r : orrery->renderTable())
        {
            if (r.location == loc)
            {
                return &r;
            }
        }
        return nullptr;
    }

} // namespace

TEST_CASE("the table holds what the draw path used to recompute")
{
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    const float scale = 0.5f;
    OrreryPtr orrery = solOrrery(game, scale);
    System *system = solSystem(game);
    REQUIRE(system != nullptr);
    REQUIRE(system->locations.size() > 0);

    // One row per location, in the same order -- the index is loop position, which is what
    // replaced the old hand-maintained Location::index and its fixed 64-entry cap.
    REQUIRE(orrery->renderTable().size() == system->locations.size());

    for (size_t idx = 0; idx < system->locations.size(); ++idx)
    {
        const Location *loc = system->locations[idx];
        const LocationRender &r = orrery->renderTable()[idx];

        REQUIRE(r.location == loc);

        // The same transform the draw loop applied per frame: absolute position, less the
        // focus, scaled, about the centre.
        const Vector2 p = loc->resolvedPosition();
        CHECK(r.screen_pos.x == doctest::Approx(640.0f + p.x * scale));
        CHECK(r.screen_pos.y == doctest::Approx(400.0f + p.y * scale));

        if (loc->type == LOCATION_TYPE_ASTEROID_BELT)
        {
            // A belt spends its two radii on the ring instead: no disc, and `radius` is the
            // ring's half-width rather than a display size.
            CHECK(r.screen_radius == doctest::Approx(0.0f));
            CHECK(r.band_inner == doctest::Approx((loc->orbital_radius - loc->radius) * scale));
            CHECK(r.band_outer == doctest::Approx((loc->orbital_radius + loc->radius) * scale));
        }
        else
        {
            CHECK(r.screen_radius == doctest::Approx(loc->radius * scale));
            CHECK(r.band_outer == doctest::Approx(0.0f));
        }
    }
}

TEST_CASE("a new game is not blank")
{
    // game_time is 0.0f until time first advances. A stamp that started at 0.0f would never
    // satisfy `game_time > last_update_time`, so the first table would never be built and
    // the orrery would draw nothing until the player unpaused.
    Game *game = loadGame();
    REQUIRE(game != nullptr);
    game->game_time = 0.0;

    OrreryPtr orrery = createOrrery((Vector2){640.0f, 400.0f}, 1.0f);
    orrery->setSystem(solSystem(game));
    orrery->update();

    CHECK_MESSAGE(orrery->renderTable().size() > 0, "first update built no table");
}

TEST_CASE("the table rebuilds when its inputs change, and not otherwise")
{
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    OrreryPtr orrery = solOrrery(game);

    Location *earth = game->locationByID(EARTH_ID);
    REQUIRE(earth != nullptr);
    REQUIRE(rowFor(orrery.get(), earth) != nullptr);

    SUBCASE("advancing time moves the rows")
    {
        const Vector2 before = rowFor(orrery.get(), earth)->screen_pos;

        // Far enough around the orbit that the move is unambiguous rather than sub-pixel.
        for (int tick = 0; tick < 200; ++tick)
        {
            game->update(1.0);
        }
        orrery->update();

        const Vector2 after = rowFor(orrery.get(), earth)->screen_pos;
        CHECK_MESSAGE(std::fabs(after.x - before.x) + std::fabs(after.y - before.y) > 1.0f,
                      "positions did not follow the advancing clock");
    }

    SUBCASE("a second update with no advance changes nothing")
    {
        // The point of the whole exercise: while paused, update() does no position work.
        // Observable only as the rows being identical, which is also what would hold if it
        // rebuilt -- so the sibling case above is what gives this one its meaning.
        const Vector2 before = rowFor(orrery.get(), earth)->screen_pos;
        orrery->update();
        const Vector2 after = rowFor(orrery.get(), earth)->screen_pos;

        CHECK(after.x == doctest::Approx(before.x));
        CHECK(after.y == doctest::Approx(before.y));
    }

    SUBCASE("a changed view rebuilds without the clock advancing")
    {
        // The case a time-only stamp would miss: zoom and pan work while paused, so the
        // view paths reset the stamp rather than waiting for game_time to move.
        const Vector2 before = rowFor(orrery.get(), earth)->screen_pos;

        orrery->focusOnLocation(earth);
        orrery->update();

        const LocationRender *row = rowFor(orrery.get(), earth);
        REQUIRE(row != nullptr);

        // Focused on Earth, Earth sits at the centre of the view.
        CHECK(row->screen_pos.x == doctest::Approx(640.0f));
        CHECK(row->screen_pos.y == doctest::Approx(400.0f));
        CHECK_MESSAGE(std::fabs(row->screen_pos.x - before.x) + std::fabs(row->screen_pos.y - before.y) > 1.0f,
                      "table was still built for the old view");
    }
}

TEST_CASE("facilities are in the table but are not hit-testable")
{
    // Facilities became locations, so they are in system->locations and therefore in the
    // table. They have no display radius, and a zero radius must mean not drawn AND not
    // clickable -- otherwise every facility sits as an invisible target on its parent body.
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    OrreryPtr orrery = solOrrery(game);

    int facilities = 0;
    for (const LocationRender &r : orrery->renderTable())
    {
        if (r.location->isFacility())
        {
            ++facilities;
            CHECK(r.screen_radius == doctest::Approx(0.0f));
            CHECK_MESSAGE(orrery->locationAt(r.screen_pos) != r.location,
                          "a facility answered a hit test");
        }
    }

    REQUIRE_MESSAGE(facilities > 0, "no facilities in the system: the test proved nothing");
}

TEST_CASE("the belt band is the lowest-precedence hit")
{
    Game *game = loadGame();
    REQUIRE(game != nullptr);

    OrreryPtr orrery = solOrrery(game);

    Location *belt = game->locationByID(BELT_ID);
    REQUIRE(belt != nullptr);
    REQUIRE(belt->type == LOCATION_TYPE_ASTEROID_BELT);

    const LocationRender *band = rowFor(orrery.get(), belt);
    REQUIRE(band != nullptr);
    REQUIRE_MESSAGE(band->band_outer > band->band_inner, "the belt got no band");

    // Focus is the origin, so the band is centred on the orrery's centre.
    const Vector2 centre = {640.0f, 400.0f};
    const float mid = (band->band_inner + band->band_outer) * 0.5f;

    SUBCASE("a point on the band alone returns the belt")
    {
        // Pick an angle where no disc is in the way, so the test is about precedence rather
        // than about where Sol's planets happen to be at time zero.
        bool tested = false;
        for (int deg = 0; deg < 360 && !tested; deg += 5)
        {
            const float rad = deg * 3.14159265f / 180.0f;
            const Vector2 point = {centre.x + std::cos(rad) * mid, centre.y + std::sin(rad) * mid};

            bool onDisc = false;
            for (const LocationRender &r : orrery->renderTable())
            {
                if (r.screen_radius > 0.0f && CheckCollisionPointCircle(point, r.screen_pos, r.screen_radius))
                {
                    onDisc = true;
                }
            }
            if (onDisc)
            {
                continue;
            }

            CHECK(orrery->locationAt(point) == belt);
            tested = true;
        }
        REQUIRE_MESSAGE(tested, "every point on the band was covered by a disc");
    }

    SUBCASE("a disc reaching into the band still wins")
    {
        // Not hypothetical in Sol: Jupiter's display radius is large enough that its disc
        // reaches inside the belt's outer edge. Without disc-first precedence the band would
        // answer first and Jupiter would be unclickable from that side.
        Location *jupiter = game->locationByID(JUPITER_ID);
        REQUIRE(jupiter != nullptr);

        const LocationRender *row = rowFor(orrery.get(), jupiter);
        REQUIRE(row != nullptr);
        REQUIRE(row->screen_radius > 0.0f);
        REQUIRE_MESSAGE(jupiter->orbital_radius - jupiter->radius < belt->orbital_radius + belt->radius,
                        "Jupiter no longer overlaps the belt: the case is hypothetical again");

        CHECK_MESSAGE(orrery->locationAt(row->screen_pos) == jupiter,
                      "the belt band swallowed a body whose disc reaches into it");
    }

    SUBCASE("a point in neither returns nothing")
    {
        // Well outside the outermost band and every orbit.
        const Vector2 point = {centre.x + band->band_outer * 4.0f, centre.y};
        CHECK(orrery->locationAt(point) == nullptr);
    }
}
