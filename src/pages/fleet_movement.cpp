#include "pages/fleet_movement.h"
#include "pages/drone_control_view.h" // full DroneFleetMarkers (shared fleet state)
#include "state/game.h"               // MAX_DRONE_FLEET_SIZE

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

extern "C"
{
#include "raylib.h"
}
// raymath.h is C++-aware (defines operator overloads); include it outside extern "C".
#include "raymath.h"

// ---------------------------------------------------------------------------
// shared tuning + helpers
// ---------------------------------------------------------------------------
namespace
{
constexpr float TWO_PI = 2.0f * PI;
constexpr int HELIX_TURNS = 5;                  // full windings per pole-to-pole pass
constexpr int WIND = 2 * HELIX_TURNS;           // phi/theta ratio (HELIX_TURNS turns per pass)
constexpr float WIND_SQ = (float)(WIND * WIND); // for the arc-length reparametrization
constexpr float LINEAR_SPEED = 50.0f;           // pixels/second of travel along a path
constexpr float FLEET_SPEED = 75.0f;            // centroid travel speed during approach
constexpr float APPROACH_RADIUS = 200.0f;       // sphere radius for a full fleet (R0)
constexpr float ENGAGE_RADIUS_MULT = 2.0f;      // engage radius ~= 2x approach
constexpr float ENGAGE_EXPAND_TIME = 5.0f;      // seconds to inflate

// scale the sphere to fleet size: a full (MAX_DRONE_FLEET_SIZE) fleet fills APPROACH_RADIUS.
float scaledRadius(int count)
{
    float frac = std::min(count, MAX_DRONE_FLEET_SIZE) / (float)MAX_DRONE_FLEET_SIZE;
    return APPROACH_RADIUS * frac;
}

// randomness for member removal. No RNG exists elsewhere in the codebase; this is the
// single, file-local source, fixed-seeded so removal order is reproducible in tests.
std::mt19937 &fleetRng()
{
    static std::mt19937 rng(0xD5049Eu);
    return rng;
}

// unit point on the spherical helix (pole axis = travel dir x). phi winds with theta;
// dir flips the winding sign so opposing fleets mirror each other.
Vector3 helixUnitPoint(float theta, float dir)
{
    float phi = dir * WIND * theta;
    float st = sinf(theta), ct = cosf(theta);
    return {ct, st * sinf(phi), st * cosf(phi)};
}

// tangent to the helix at theta (unit sphere), used to seed a smooth orbit heading.
Vector3 helixUnitTangent(float theta, float dir)
{
    float phi = dir * WIND * theta;
    float st = sinf(theta), ct = cosf(theta);
    return {-st,
            ct * sinf(phi) + st * dir * WIND * cosf(phi),
            ct * cosf(phi) - st * dir * WIND * sinf(phi)};
}

// advance a helix arc-parameter at constant linear (screen) speed. ds/dtheta on the
// sphere is radius*sqrt(1 + WIND^2 sin^2 theta); stepping theta by v*dt/(ds/dtheta)
// gives a constant pace along the wire and slows the winding as radius grows.
float advanceHelixParam(float theta, float radius, float delta)
{
    float s = sinf(theta);
    float ds_dtheta = radius * sqrtf(1.0f + WIND_SQ * s * s);
    if (ds_dtheta > 1e-4f)
    {
        theta = fmodf(theta + (LINEAR_SPEED * delta) / ds_dtheta, TWO_PI);
    }
    return theta;
}
} // namespace

// process-wide flocking coefficients (external linkage; see fleet_movement.h). Mutable so
// the debug UI can tune the boids live; defaults reproduce the original constant values.
FlockingParams &flockingParams()
{
    static FlockingParams params;
    return params;
}

// ---------------------------------------------------------------------------
// HelicalMovement: every live marker slides along one shared spherical-helix wire.
// ---------------------------------------------------------------------------
namespace
{
class HelicalMovement : public MovementPattern
{
    std::vector<float> path_length; // slot-indexed helix arc parameter

    void spherePoint(DroneFleetMarkers &f, int i, float radius);

public:
    explicit HelicalMovement(int count);
    void stepApproach(DroneFleetMarkers &f, float delta) override;
    void stepEngage(DroneFleetMarkers &f, float delta) override;
    void stepRetreat(DroneFleetMarkers &f, float delta) override;
};

HelicalMovement::HelicalMovement(int count)
{
    path_length.resize(count);

    // spread the per-marker arc parameter over one full up-and-down cycle (0..2*PI of theta)
    float inc = (count > 0) ? (TWO_PI / count) : 0.0f;
    for (int i = 0; i < count; i++)
    {
        path_length[i] = i * inc;
    }
}

void HelicalMovement::spherePoint(DroneFleetMarkers &f, int i, float radius)
{
    float theta = fmodf(path_length[i], TWO_PI);
    if (theta < 0.0f)
    {
        theta += TWO_PI;
    }
    Vector3 p = helixUnitPoint(theta, f.approach_dir);
    f.slot_pos[i].x = f.fleet_position.x + radius * p.x; // axial position along travel dir
    f.slot_pos[i].y = f.fleet_position.y + radius * p.y; // vertical winding (visible)
    f.slot_depth[i] = 0.5f - 0.5f * p.z;                 // p.z into screen: 1 near, 0 far
}

void HelicalMovement::stepApproach(DroneFleetMarkers &f, float delta)
{
    for (int i = 0; i < f.capacity; i++)
    {
        if (!f.live[i])
        {
            continue;
        }
        path_length[i] = advanceHelixParam(path_length[i], f.base_radius, delta);
        spherePoint(f, i, f.base_radius);
    }
}

void HelicalMovement::stepEngage(DroneFleetMarkers &f, float delta)
{
    for (int i = 0; i < f.capacity; i++)
    {
        if (!f.live[i])
        {
            continue;
        }
        path_length[i] = advanceHelixParam(path_length[i], f.sphere_radius, delta);
        spherePoint(f, i, f.sphere_radius);
    }
}

void HelicalMovement::stepRetreat(DroneFleetMarkers &f, float delta)
{
    for (int i = 0; i < f.capacity; i++)
    {
        if (f.live[i])
        {
            f.slot_pos[i].x -= FLEET_SPEED * delta * f.approach_dir;
        }
    }
}
} // namespace

// ---------------------------------------------------------------------------
// FlockingMovement: virtual leader points move on the sphere; drone markers are
// boids that seek their leader while separating from their flockmates.
// ---------------------------------------------------------------------------
namespace
{
constexpr int FLOCK_SIZE = 35; // target members per flock (splits at init only)
// The per-member coefficients (seek/separation/speed/damping/precession) are now runtime-
// tunable via flockingParams() (see fleet_movement.h) so the debug UI can adjust them live.

struct Leader
{
    float path_length; // approach-phase helix parameter
    Vector3 p;         // unit position on the sphere
    Vector3 h;          // unit tangent heading (engage-phase orbit)
    Vector2 screen;     // projected battle-area-local position
    Vector2 screen_prev; // previous frame's projection, for screen-space heading
    float depth;        // 0..1 nearness
};

struct Member
{
    Vector2 pos;
    Vector2 vel;
    int flock;
    float depth;
};

class FlockingMovement : public MovementPattern
{
    std::vector<Leader> leaders;
    std::vector<Member> members; // slot-indexed

    void projectLeader(DroneFleetMarkers &f, Leader &L, float radius);
    void updateMembers(DroneFleetMarkers &f, float delta);
    void writeSlots(DroneFleetMarkers &f);

public:
    explicit FlockingMovement(int count);
    void onConfigure(DroneFleetMarkers &f) override;
    void stepApproach(DroneFleetMarkers &f, float delta) override;
    void onBeginEngage(DroneFleetMarkers &f) override;
    void stepEngage(DroneFleetMarkers &f, float delta) override;
    void stepRetreat(DroneFleetMarkers &f, float delta) override;
    void postStep(DroneFleetMarkers &f, float delta) override;
};

FlockingMovement::FlockingMovement(int count)
{
    // split into flocks: round total / FLOCK_SIZE (at least one flock while any members).
    int numFlocks = (count > 0) ? std::max(1, (count + FLOCK_SIZE / 2) / FLOCK_SIZE) : 0;
    leaders.resize(numFlocks);
    float linc = (numFlocks > 0) ? (TWO_PI / numFlocks) : 0.0f;
    for (int fdx = 0; fdx < numFlocks; fdx++)
    {
        leaders[fdx].path_length = fdx * linc; // spread leaders around the helix
        leaders[fdx].p = {1.0f, 0.0f, 0.0f};
        leaders[fdx].h = {0.0f, 1.0f, 0.0f};
        leaders[fdx].screen = {0.0f, 0.0f};
        leaders[fdx].screen_prev = {0.0f, 0.0f};
        leaders[fdx].depth = 1.0f;
    }

    // assign members to flocks in roughly-equal contiguous groups.
    members.resize(count);
    for (int i = 0; i < count; i++)
    {
        members[i].flock = (numFlocks > 0) ? (i * numFlocks / count) : 0;
        members[i].pos = {0.0f, 0.0f};
        members[i].vel = {0.0f, 0.0f};
        members[i].depth = 1.0f;
    }
}

void FlockingMovement::projectLeader(DroneFleetMarkers &f, Leader &L, float radius)
{
    L.screen = {f.fleet_position.x + radius * L.p.x, f.fleet_position.y + radius * L.p.y};
    L.depth = 0.5f - 0.5f * L.p.z;
}

void FlockingMovement::onConfigure(DroneFleetMarkers &f)
{
    // seat leaders on the approach helix, then scatter members around their leader so the
    // separation force has something to work with (rather than all starting coincident).
    for (auto &L : leaders)
    {
        L.p = helixUnitPoint(L.path_length, f.approach_dir);
        projectLeader(f, L, f.base_radius);
        L.screen_prev = L.screen; // seed heading history so the first frame isn't a jump
    }
    for (int i = 0; i < f.capacity; i++)
    {
        Vector2 c = leaders.empty() ? f.fleet_position : leaders[members[i].flock].screen;
        float ang = i * 2.39996323f; // golden angle -> even angular scatter
        float rad = 10.0f + (float)(i % 7);
        members[i].pos = {c.x + cosf(ang) * rad, c.y + sinf(ang) * rad};
        members[i].vel = {0.0f, 0.0f};
        members[i].depth = leaders.empty() ? 1.0f : leaders[members[i].flock].depth;
    }
}

void FlockingMovement::stepApproach(DroneFleetMarkers &f, float delta)
{
    for (auto &L : leaders)
    {
        L.path_length = advanceHelixParam(L.path_length, f.base_radius, delta);
        L.p = helixUnitPoint(L.path_length, f.approach_dir);
        projectLeader(f, L, f.base_radius);
    }
}

void FlockingMovement::onBeginEngage(DroneFleetMarkers &f)
{
    // continue smoothly: seed each orbit heading from the helix tangent, made perpendicular
    // to the leader's position and unit length.
    for (auto &L : leaders)
    {
        Vector3 t = helixUnitTangent(L.path_length, f.approach_dir);
        Vector3 h = Vector3Subtract(t, Vector3Scale(L.p, Vector3DotProduct(t, L.p)));
        float len = Vector3Length(h);
        L.h = (len > 1e-5f) ? Vector3Scale(h, 1.0f / len)
                            : Vector3Normalize(Vector3CrossProduct(L.p, {0.0f, 0.0f, 1.0f}));
    }
}

void FlockingMovement::stepEngage(DroneFleetMarkers &f, float delta)
{
    // constant linear speed along the sphere: arc = radius * dAlpha => dAlpha = v*dt/radius.
    float dAlpha = (LINEAR_SPEED * delta) / std::max(f.sphere_radius, 1e-3f);

    for (size_t fdx = 0; fdx < leaders.size(); ++fdx)
    {
        Leader &L = leaders[fdx];

        // move along the current great circle (rotate position + heading about the orbit normal).
        Vector3 axis = Vector3CrossProduct(L.p, L.h);
        float al = Vector3Length(axis);
        if (al < 1e-5f)
        {
            L.h = Vector3Normalize(Vector3CrossProduct(L.p, {0.0f, 0.0f, 1.0f}));
            axis = Vector3CrossProduct(L.p, L.h);
            al = Vector3Length(axis);
        }
        axis = Vector3Scale(axis, 1.0f / al);
        L.p = Vector3Normalize(Vector3RotateByAxisAngle(L.p, axis, dAlpha));
        L.h = Vector3Normalize(Vector3RotateByAxisAngle(L.h, axis, dAlpha));

        // small per-leader precession of the heading -> the orbit plane drifts, tracing
        // rosette-like patterns over the sphere instead of a fixed great circle.
        float precess = flockingParams().orbit_precess * (0.4f + 0.2f * (float)fdx) * delta;
        L.h = Vector3Normalize(Vector3RotateByAxisAngle(L.h, L.p, precess));

        // re-orthonormalize the heading against position to keep numerical drift in check.
        L.h = Vector3Normalize(Vector3Subtract(L.h, Vector3Scale(L.p, Vector3DotProduct(L.p, L.h))));

        projectLeader(f, L, f.sphere_radius);
    }
}

void FlockingMovement::stepRetreat(DroneFleetMarkers &f, float delta)
{
    for (auto &L : leaders)
    {
        L.screen.x -= FLEET_SPEED * delta * f.approach_dir;
    }
}

void FlockingMovement::updateMembers(DroneFleetMarkers &f, float delta)
{
    // snapshot the runtime-tunable coefficients once (not inside the O(n^2) loop).
    const FlockingParams &fp = flockingParams();
    const float sep_radius_sq = fp.sep_radius * fp.sep_radius;
    const int numFlocks = (int)leaders.size();

    // --- pre-pass 1: per-flock heading (screen-space travel dir), centroid, and apex slot.
    // apexSlot = lowest live slot in the flock; since a flock's members occupy a contiguous
    // increasing slot range, the first live slot we meet in slot order is its apex. If that
    // member dies, the next one inherits the apex next frame (automatic promotion).
    std::vector<Vector2> heading(numFlocks, {f.approach_dir, 0.0f});
    std::vector<Vector2> centroid(numFlocks, {0.0f, 0.0f});
    std::vector<int> flockLive(numFlocks, 0);
    std::vector<int> apexSlot(numFlocks, -1);
    for (int fl = 0; fl < numFlocks; fl++)
    {
        Leader &L = leaders[fl];
        Vector2 d = Vector2Subtract(L.screen, L.screen_prev);
        if ((d.x * d.x + d.y * d.y) > 1e-6f)
        {
            heading[fl] = Vector2Normalize(d);
        }
        L.screen_prev = L.screen;
    }
    for (int i = 0; i < f.capacity; i++)
    {
        if (!f.live[i] || numFlocks == 0)
        {
            continue;
        }
        int fl = members[i].flock;
        if (apexSlot[fl] < 0)
        {
            apexSlot[fl] = i;
        }
        centroid[fl] = Vector2Add(centroid[fl], members[i].pos);
        flockLive[fl]++;
    }
    for (int fl = 0; fl < numFlocks; fl++)
    {
        if (flockLive[fl] > 0)
        {
            centroid[fl] = Vector2Scale(centroid[fl], 1.0f / (float)flockLive[fl]);
        }
    }

    // --- pre-pass 2 (implicit): rank within flock, incremented in slot order (apex = 0).
    std::vector<int> rank(numFlocks, 0);

    // separation is within-flock only (members keep distance from their own flockmates).
    for (int i = 0; i < f.capacity; i++)
    {
        if (!f.live[i])
        {
            continue;
        }
        Member &m = members[i];

        Vector2 acc;
        if (numFlocks == 0)
        {
            acc = Vector2Scale(Vector2Subtract(f.fleet_position, m.pos), fp.seek_gain);
        }
        else
        {
            int fl = m.flock;
            Vector2 leaderPos = leaders[fl].screen;
            int r = rank[fl]++;
            if (i == apexSlot[fl])
            {
                // apex: hug the leader guide so a marker leads the flock.
                acc = Vector2Scale(Vector2Subtract(leaderPos, m.pos), fp.seek_gain);
            }
            else
            {
                // trailing anchor: a point behind the leader, pushed further back by rank so
                // the flock strings out into a tapering plume; plus cohesion to flockmates.
                Vector2 back = {-heading[fl].x, -heading[fl].y};
                Vector2 anchor = Vector2Add(leaderPos, Vector2Scale(back, fp.trail_distance * (float)r));
                acc = Vector2Scale(Vector2Subtract(anchor, m.pos), fp.trail_gain);
                acc = Vector2Add(acc, Vector2Scale(Vector2Subtract(centroid[fl], m.pos), fp.cohesion_gain));
            }
        }

        Vector2 sep = {0.0f, 0.0f};
        for (int j = 0; j < f.capacity; j++)
        {
            if (j == i || !f.live[j] || members[j].flock != m.flock)
            {
                continue;
            }
            Vector2 d = Vector2Subtract(m.pos, members[j].pos);
            float dist2 = d.x * d.x + d.y * d.y;
            if (dist2 < sep_radius_sq && dist2 > 1e-4f)
            {
                sep = Vector2Add(sep, Vector2Scale(d, 1.0f / sqrtf(dist2))); // unit push away
            }
        }
        acc = Vector2Add(acc, Vector2Scale(sep, fp.sep_gain));

        m.vel = Vector2Add(m.vel, Vector2Scale(acc, delta));
        m.vel = Vector2Scale(m.vel, 1.0f - std::min(1.0f, fp.member_damping * delta));
        float sp = Vector2Length(m.vel);
        if (sp > fp.member_maxspeed)
        {
            m.vel = Vector2Scale(m.vel, fp.member_maxspeed / sp);
        }
        m.pos = Vector2Add(m.pos, Vector2Scale(m.vel, delta));
        m.depth = leaders.empty() ? 1.0f : leaders[m.flock].depth;
    }
}

void FlockingMovement::writeSlots(DroneFleetMarkers &f)
{
    for (int i = 0; i < f.capacity; i++)
    {
        if (f.live[i])
        {
            f.slot_pos[i] = members[i].pos;
            f.slot_depth[i] = members[i].depth;
        }
    }
}

void FlockingMovement::postStep(DroneFleetMarkers &f, float delta)
{
    updateMembers(f, delta);
    writeSlots(f);
}
} // namespace

// ---------------------------------------------------------------------------
// DroneFleetMarkers: shared fleet state + state-machine driver (defined here so it can
// use the file-local tuning constants, scaledRadius, and the RNG). render() lives in
// drone_control_view.cpp.
// ---------------------------------------------------------------------------
void DroneFleetMarkers::initialise(int count, Color c, MovementPatternType type)
{
    color = c;
    state = DFS_APPROACHING;
    capacity = count;
    live_count = count;
    out_count = 0;
    base_radius = scaledRadius(count);
    sphere_radius = base_radius;
    engage_timer = 0.0f;
    engage_x = 0.0f;
    approach_dir = 1.0f;
    fleet_position = {0.0f, 0.0f};

    live.assign(count, 1);
    slot_pos.assign(count, {0.0f, 0.0f});
    slot_depth.assign(count, 1.0f);
    out_pos.assign(count, {0.0f, 0.0f});
    out_depth.assign(count, 1.0f);
    scratch_idx.clear();
    scratch_idx.reserve(count);

    motion = makeMovementPattern(type, count);
}

void DroneFleetMarkers::setApproach(Vector2 start, float target_x, float dir)
{
    fleet_position = start;
    engage_x = target_x;
    approach_dir = dir;
    if (motion)
    {
        motion->onConfigure(*this);
    }
}

void DroneFleetMarkers::prewarm(int iterations, float dt)
{
    // Run the per-member dynamics in place (no phase step, no fleet translation) so the
    // formation settles before the first render — avoids the "explosion" from members
    // starting bunched inside the separation radius. Deterministic patterns (helical) have
    // an empty postStep, so this is a cheap no-op for them.
    if (!motion)
    {
        return;
    }
    for (int i = 0; i < iterations; i++)
    {
        motion->postStep(*this, dt);
    }
    compact();
}

void DroneFleetMarkers::update(float delta)
{
    if (!motion)
    {
        return;
    }

    switch (state)
    {
    case DFS_APPROACHING:
        fleet_position.x += FLEET_SPEED * delta * approach_dir;
        motion->stepApproach(*this, delta);
        if (approach_dir * (fleet_position.x - engage_x) >= 0.0f)
        {
            state = DFS_ENGAGING;
            engage_timer = 0.0f;
            motion->onBeginEngage(*this);
        }
        break;
    case DFS_ENGAGING:
    {
        engage_timer += delta;
        float u = std::min(engage_timer / ENGAGE_EXPAND_TIME, 1.0f);
        float ease = u * u * (3.0f - 2.0f * u); // smoothstep
        sphere_radius = base_radius * (1.0f + (ENGAGE_RADIUS_MULT - 1.0f) * ease);
        motion->stepEngage(*this, delta);
        break;
    }
    default: // DFS_RETREATING
        fleet_position.x -= FLEET_SPEED * delta * approach_dir;
        motion->stepRetreat(*this, delta);
        break;
    }

    motion->postStep(*this, delta);
    compact();
}

void DroneFleetMarkers::compact()
{
    int o = 0;
    for (int i = 0; i < capacity; i++)
    {
        if (live[i])
        {
            out_pos[o] = slot_pos[i];
            out_depth[o] = slot_depth[i];
            ++o;
        }
    }
    out_count = o;
}

void DroneFleetMarkers::killRandom(int k)
{
    if (k <= 0 || live_count <= 0)
    {
        return;
    }
    k = std::min(k, live_count);

    // gather live slot indices, then partial Fisher-Yates: pick k distinct live slots to kill.
    scratch_idx.clear();
    for (int i = 0; i < capacity; i++)
    {
        if (live[i])
        {
            scratch_idx.push_back(i);
        }
    }
    for (int j = 0; j < k; j++)
    {
        std::uniform_int_distribution<int> d(j, (int)scratch_idx.size() - 1);
        std::swap(scratch_idx[j], scratch_idx[d(fleetRng())]);
        live[scratch_idx[j]] = 0;
    }
    live_count -= k;
}

// ---------------------------------------------------------------------------
// factory
// ---------------------------------------------------------------------------
std::unique_ptr<MovementPattern> makeMovementPattern(MovementPatternType type, int count)
{
    switch (type)
    {
    case MP_FLOCKING:
        return std::make_unique<FlockingMovement>(count);
    case MP_HELICAL:
    default:
        return std::make_unique<HelicalMovement>(count);
    }
}
