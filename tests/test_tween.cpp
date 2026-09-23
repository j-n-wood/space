// Tween: the real-time interpolation behind UI animation.
//
// It is deliberately free of raylib and of game state, so all of it is reachable here. Two
// properties carry most of the weight: a zero duration means "already finished" rather than
// "divide by zero", which is what lets a caller snap to an end state without special-casing;
// and the timer never runs past the duration, so value() cannot overshoot the endpoint.

#include "doctest.h"
#include "../include/pages/tween.h"

TEST_CASE("a default tween is idle")
{
    // Nothing animates until it is reset or started -- a page can hold an array of these and
    // only the ones it has asked for will move.
    Tween t;

    CHECK(t.active == false);
    CHECK(t.timer == doctest::Approx(0.0f));
    CHECK(t.progress() == doctest::Approx(0.0f));
    CHECK(t.value() == doctest::Approx(0.0f)); // start_value

    CHECK_MESSAGE(t.update(1.0f) == false, "an idle tween reported completion");
    CHECK_MESSAGE(t.timer == doctest::Approx(0.0f), "an idle tween advanced its timer");
}

TEST_CASE("reset arms the tween and interpolates over the duration")
{
    Tween t;
    t.reset(0.0f, 1.0f, 2.0f);

    REQUIRE(t.active);
    CHECK(t.progress() == doctest::Approx(0.0f));
    CHECK(t.value() == doctest::Approx(0.0f));

    CHECK(t.update(0.5f) == false);
    CHECK(t.progress() == doctest::Approx(0.25f));
    CHECK(t.value() == doctest::Approx(0.25f));

    CHECK(t.update(0.5f) == false);
    CHECK(t.progress() == doctest::Approx(0.5f));
    CHECK(t.value() == doctest::Approx(0.5f));
}

TEST_CASE("value interpolates between the endpoints, in either direction")
{
    // The door case: closing runs 0 -> 1 and opening runs 1 -> 0, so the same type has to
    // handle a descending range without the caller inverting anything.
    SUBCASE("ascending")
    {
        Tween t;
        t.reset(0.0f, 1.0f, 1.0f);
        t.update(0.25f);
        CHECK(t.value() == doctest::Approx(0.25f));
    }

    SUBCASE("descending")
    {
        Tween t;
        t.reset(1.0f, 0.0f, 1.0f);
        t.update(0.25f);
        CHECK(t.progress() == doctest::Approx(0.25f)); // progress always runs 0 -> 1
        CHECK(t.value() == doctest::Approx(0.75f));    // ... while the value runs back down
    }

    SUBCASE("an arbitrary range")
    {
        Tween t;
        t.reset(100.0f, 300.0f, 4.0f);
        t.update(1.0f);
        CHECK(t.value() == doctest::Approx(150.0f));
    }
}

TEST_CASE("the timer stops at the duration")
{
    // Without the clamp, value() would sail past end_value and a door would keep travelling
    // after it had shut.
    Tween t;
    t.reset(0.0f, 1.0f, 0.5f);

    CHECK_MESSAGE(t.update(5.0f) == true, "a tween past its duration did not report completion");
    CHECK(t.timer == doctest::Approx(0.5f));
    CHECK(t.progress() == doctest::Approx(1.0f));
    CHECK(t.value() == doctest::Approx(1.0f));
}

TEST_CASE("completion is a level, not an edge")
{
    // update() answers "is it finished", not "did it just finish" -- it keeps returning true
    // on every later call. A caller wanting a one-shot has to track that itself.
    Tween t;
    t.reset(0.0f, 1.0f, 1.0f);

    REQUIRE(t.update(2.0f) == true);
    CHECK(t.update(0.016f) == true);
    CHECK(t.update(0.016f) == true);
    CHECK_MESSAGE(t.value() == doctest::Approx(1.0f), "the value drifted past the endpoint");
}

TEST_CASE("a zero duration means already finished")
{
    // How a caller snaps to an end state with no animation -- activate() uses this so a page
    // opened mid-animation shows the finished state rather than replaying it. The tween is
    // left inactive, and both readers report the end.
    Tween t;
    t.reset(0.0f, 1.0f, 0.0f);

    CHECK_MESSAGE(t.active == false, "a zero-duration tween armed itself");
    CHECK_MESSAGE(t.value() == doctest::Approx(1.0f), "a zero-duration tween did not snap to the end");
    CHECK_MESSAGE(t.progress() == doctest::Approx(1.0f), "progress() divided by a zero duration");

    SUBCASE("and snapping the other way lands on that end instead")
    {
        t.reset(1.0f, 0.0f, 0.0f);
        CHECK(t.value() == doctest::Approx(0.0f));
    }

    SUBCASE("and it does not move")
    {
        CHECK(t.update(1.0f) == false);
        CHECK(t.value() == doctest::Approx(1.0f));
    }
}

TEST_CASE("start restarts the run")
{
    Tween t;
    t.reset(0.0f, 1.0f, 1.0f);
    t.update(0.75f);
    REQUIRE(t.progress() == doctest::Approx(0.75f));

    SUBCASE("keeping the duration when none is given")
    {
        t.start();
        CHECK(t.duration == doctest::Approx(1.0f));
        CHECK(t.progress() == doctest::Approx(0.0f));
        CHECK(t.active);
    }

    SUBCASE("or taking a new one")
    {
        t.start(4.0f);
        CHECK(t.duration == doctest::Approx(4.0f));
        CHECK(t.progress() == doctest::Approx(0.0f));
        CHECK(t.active);
    }

    SUBCASE("a zero duration leaves it finished rather than running")
    {
        t.start(0.0f);
        CHECK(t.active == false);
        CHECK(t.value() == doctest::Approx(1.0f));
    }
}

TEST_CASE("reversing mid-run starts the new direction from the beginning")
{
    // The page flips direction by calling reset with the endpoints swapped. That restarts the
    // timer, so a door interrupted half way takes a full duration to travel back -- it does
    // not resume from where it had got to. Pinned because it is the visible consequence.
    Tween t;
    t.reset(0.0f, 1.0f, 1.0f);
    t.update(0.5f);
    REQUIRE(t.value() == doctest::Approx(0.5f));

    t.reset(1.0f, 0.0f, 1.0f);

    CHECK_MESSAGE(t.value() == doctest::Approx(1.0f), "the reversed tween did not begin at the far end");
    CHECK(t.timer == doctest::Approx(0.0f));

    t.update(1.0f);
    CHECK(t.value() == doctest::Approx(0.0f));
}
