// Unit tests for the drone fleet motion + member-removal mechanism.
// Pure logic only: no render(), so no raylib window is required.
#include "doctest.h"
#include "../include/pages/drone_control_view.h"

#include <cmath>

namespace
{
const MovementPatternType kPatterns[] = {MP_HELICAL, MP_FLOCKING};

// A fleet seated at the start of an approach, advanced a few frames.
void runFrames(DroneFleetMarkers &f, int frames)
{
    f.setApproach({10.0f, 100.0f}, 200.0f, +1.0f);
    for (int i = 0; i < frames; i++)
    {
        f.update(0.016f);
    }
}

bool allFinite(const DroneFleetMarkers &f)
{
    for (int i = 0; i < f.out_count; i++)
    {
        if (!std::isfinite(f.out_pos[i].x) || !std::isfinite(f.out_pos[i].y) ||
            !std::isfinite(f.out_depth[i]))
        {
            return false;
        }
    }
    return true;
}
} // namespace

TEST_CASE("Fleet initialise sizes buffers and packs on update")
{
    for (MovementPatternType type : kPatterns)
    {
        CAPTURE(type);
        DroneFleetMarkers f;
        f.initialise(20, Color{255, 0, 0, 255}, type);

        CHECK(f.capacity == 20);
        CHECK(f.live_count == 20);
        CHECK(f.out_count == 0); // nothing packed until the first update

        runFrames(f, 5);
        CHECK(f.out_count == 20);
        CHECK(allFinite(f));
    }
}

TEST_CASE("Fleet killRandom reduces live and packed counts")
{
    for (MovementPatternType type : kPatterns)
    {
        CAPTURE(type);
        DroneFleetMarkers f;
        f.initialise(20, Color{0, 0, 255, 255}, type);

        f.killRandom(3);
        CHECK(f.live_count == 17);

        runFrames(f, 3);
        CHECK(f.out_count == 17);
        CHECK(allFinite(f));
    }
}

TEST_CASE("Fleet setLiveCount targets an exact live count")
{
    for (MovementPatternType type : kPatterns)
    {
        CAPTURE(type);
        DroneFleetMarkers f;
        f.initialise(20, Color{0, 255, 0, 255}, type);

        f.setLiveCount(5);
        CHECK(f.live_count == 5);
        runFrames(f, 2);
        CHECK(f.out_count == 5);

        f.setLiveCount(0);
        CHECK(f.live_count == 0);
        f.update(0.016f); // must be safe with no live members
        CHECK(f.out_count == 0);
    }
}

TEST_CASE("Fleet killRandom clamps to available members")
{
    for (MovementPatternType type : kPatterns)
    {
        CAPTURE(type);
        DroneFleetMarkers f;
        f.initialise(10, Color{255, 255, 0, 255}, type);

        f.killRandom(999);
        CHECK(f.live_count == 0);

        f.update(0.016f);
        CHECK(f.out_count == 0);

        f.killRandom(1); // no-op once empty
        CHECK(f.live_count == 0);
    }
}
