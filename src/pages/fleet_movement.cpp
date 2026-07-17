#include "pages/fleet_movement.h"
#include "state/game.h" // MAX_DRONE_FLEET_SIZE

#include <algorithm>
#include <cmath>
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

// ---------------------------------------------------------------------------
// HelicalMovement: every marker slides along one shared spherical-helix wire.
// ---------------------------------------------------------------------------
namespace
{
class HelicalMovement : public MovementPattern
{
    int n;
    DroneFleetState state = DFS_APPROACHING;
    float base_radius;
    float sphere_radius;
    float engage_timer = 0.0f;
    float engage_x = 0.0f;
    float approach_dir = 1.0f;
    Vector2 fleet_position = {0.0f, 0.0f};
    std::vector<float> path_length;
    std::vector<Vector2> position;
    std::vector<float> depth;

    void spherePoint(int i, float radius);

public:
    explicit HelicalMovement(int count);
    void configure(Vector2 start, float target_x, float dir) override;
    void update(float delta) override;
    int count() const override { return n; }
    const Vector2 *positions() const override { return position.data(); }
    const float *depths() const override { return depth.data(); }
    DroneFleetState phase() const override { return state; }
};

HelicalMovement::HelicalMovement(int count) : n(count)
{
    base_radius = scaledRadius(count);
    sphere_radius = base_radius;
    path_length.resize(count);
    position.assign(count, {0.0f, 0.0f});
    depth.assign(count, 1.0f);

    // spread the per-marker arc parameter over one full up-and-down cycle (0..2*PI of theta)
    float inc = (count > 0) ? (TWO_PI / count) : 0.0f;
    for (int i = 0; i < count; i++)
    {
        path_length[i] = i * inc;
    }
}

void HelicalMovement::configure(Vector2 start, float target_x, float dir)
{
    fleet_position = start;
    engage_x = target_x;
    approach_dir = dir;
}

void HelicalMovement::spherePoint(int i, float radius)
{
    float theta = fmodf(path_length[i], TWO_PI);
    if (theta < 0.0f)
    {
        theta += TWO_PI;
    }
    Vector3 p = helixUnitPoint(theta, approach_dir);
    position[i].x = fleet_position.x + radius * p.x; // axial position along travel dir
    position[i].y = fleet_position.y + radius * p.y; // vertical winding (visible)
    depth[i] = 0.5f - 0.5f * p.z;                    // p.z into screen: 1 near, 0 far
}

void HelicalMovement::update(float delta)
{
    if (state == DFS_APPROACHING)
    {
        fleet_position.x += FLEET_SPEED * delta * approach_dir;
        for (int i = 0; i < n; i++)
        {
            path_length[i] = advanceHelixParam(path_length[i], base_radius, delta);
            spherePoint(i, base_radius);
        }
        if (approach_dir * (fleet_position.x - engage_x) >= 0.0f)
        {
            state = DFS_ENGAGING;
            engage_timer = 0.0f;
        }
    }
    else if (state == DFS_ENGAGING)
    {
        engage_timer += delta;
        float u = std::min(engage_timer / ENGAGE_EXPAND_TIME, 1.0f);
        float ease = u * u * (3.0f - 2.0f * u); // smoothstep
        sphere_radius = base_radius * (1.0f + (ENGAGE_RADIUS_MULT - 1.0f) * ease);
        for (int i = 0; i < n; i++)
        {
            path_length[i] = advanceHelixParam(path_length[i], sphere_radius, delta);
            spherePoint(i, sphere_radius);
        }
    }
    else // DFS_RETREATING
    {
        fleet_position.x -= FLEET_SPEED * delta * approach_dir;
        for (int i = 0; i < n; i++)
        {
            position[i].x -= FLEET_SPEED * delta * approach_dir;
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
constexpr int FLOCK_SIZE = 35;         // target members per flock (splits at init only)
constexpr float ORBIT_PRECESS = 0.6f;  // radians/sec heading precession (varies per leader)
constexpr float SEEK_GAIN = 4.0f;      // pull of a member toward its leader
constexpr float SEP_GAIN = 90.0f;      // push away from nearby flockmates
constexpr float SEP_RADIUS = 16.0f;    // separation neighbourhood (px)
constexpr float MEMBER_MAXSPEED = 260.0f;
constexpr float MEMBER_DAMPING = 1.2f; // velocity damping coefficient (per second)

struct Leader
{
    float path_length; // approach-phase helix parameter
    Vector3 p;         // unit position on the sphere
    Vector3 h;         // unit tangent heading (engage-phase orbit)
    Vector2 screen;    // projected battle-area-local position
    float depth;       // 0..1 nearness
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
    int n;
    DroneFleetState state = DFS_APPROACHING;
    float base_radius;
    float sphere_radius;
    float engage_timer = 0.0f;
    float engage_x = 0.0f;
    float approach_dir = 1.0f;
    Vector2 fleet_position = {0.0f, 0.0f};
    std::vector<Leader> leaders;
    std::vector<Member> members;
    std::vector<Vector2> position; // outputs (member positions)
    std::vector<float> depth;

    void projectLeader(Leader &L, float radius);
    void updateLeadersApproach(float delta);
    void beginEngage();
    void updateLeadersEngage(float delta);
    void updateMembers(float delta);
    void writeOutputs();

public:
    explicit FlockingMovement(int count);
    void configure(Vector2 start, float target_x, float dir) override;
    void update(float delta) override;
    int count() const override { return n; }
    const Vector2 *positions() const override { return position.data(); }
    const float *depths() const override { return depth.data(); }
    DroneFleetState phase() const override { return state; }
};

FlockingMovement::FlockingMovement(int count) : n(count)
{
    base_radius = scaledRadius(count);
    sphere_radius = base_radius;

    // split into flocks: round total / FLOCK_SIZE (at least one flock while any members).
    int numFlocks = (count > 0) ? std::max(1, (count + FLOCK_SIZE / 2) / FLOCK_SIZE) : 0;
    leaders.resize(numFlocks);
    float linc = (numFlocks > 0) ? (TWO_PI / numFlocks) : 0.0f;
    for (int f = 0; f < numFlocks; f++)
    {
        leaders[f].path_length = f * linc; // spread leaders around the helix
        leaders[f].p = {1.0f, 0.0f, 0.0f};
        leaders[f].h = {0.0f, 1.0f, 0.0f};
        leaders[f].screen = {0.0f, 0.0f};
        leaders[f].depth = 1.0f;
    }

    // assign members to flocks in roughly-equal contiguous groups.
    members.resize(count);
    position.assign(count, {0.0f, 0.0f});
    depth.assign(count, 1.0f);
    for (int i = 0; i < count; i++)
    {
        members[i].flock = (numFlocks > 0) ? (i * numFlocks / count) : 0;
        members[i].pos = {0.0f, 0.0f};
        members[i].vel = {0.0f, 0.0f};
        members[i].depth = 1.0f;
    }
}

void FlockingMovement::projectLeader(Leader &L, float radius)
{
    L.screen = {fleet_position.x + radius * L.p.x, fleet_position.y + radius * L.p.y};
    L.depth = 0.5f - 0.5f * L.p.z;
}

void FlockingMovement::configure(Vector2 start, float target_x, float dir)
{
    fleet_position = start;
    engage_x = target_x;
    approach_dir = dir;

    // seat leaders on the approach helix, then scatter members around their leader so the
    // separation force has something to work with (rather than all starting coincident).
    for (auto &L : leaders)
    {
        L.p = helixUnitPoint(L.path_length, dir);
        projectLeader(L, base_radius);
    }
    for (int i = 0; i < n; i++)
    {
        Vector2 c = leaders.empty() ? start : leaders[members[i].flock].screen;
        float ang = i * 2.39996323f; // golden angle -> even angular scatter
        float rad = 10.0f + (float)(i % 7);
        members[i].pos = {c.x + cosf(ang) * rad, c.y + sinf(ang) * rad};
        members[i].vel = {0.0f, 0.0f};
        members[i].depth = leaders.empty() ? 1.0f : leaders[members[i].flock].depth;
    }
}

void FlockingMovement::updateLeadersApproach(float delta)
{
    for (auto &L : leaders)
    {
        L.path_length = advanceHelixParam(L.path_length, base_radius, delta);
        L.p = helixUnitPoint(L.path_length, approach_dir);
        projectLeader(L, base_radius);
    }
}

void FlockingMovement::beginEngage()
{
    // continue smoothly: seed each orbit heading from the helix tangent, made perpendicular
    // to the leader's position and unit length.
    for (auto &L : leaders)
    {
        Vector3 t = helixUnitTangent(L.path_length, approach_dir);
        Vector3 h = Vector3Subtract(t, Vector3Scale(L.p, Vector3DotProduct(t, L.p)));
        float len = Vector3Length(h);
        L.h = (len > 1e-5f) ? Vector3Scale(h, 1.0f / len)
                            : Vector3Normalize(Vector3CrossProduct(L.p, {0.0f, 0.0f, 1.0f}));
    }
}

void FlockingMovement::updateLeadersEngage(float delta)
{
    // constant linear speed along the sphere: arc = radius * dAlpha => dAlpha = v*dt/radius.
    float dAlpha = (LINEAR_SPEED * delta) / std::max(sphere_radius, 1e-3f);

    for (size_t f = 0; f < leaders.size(); ++f)
    {
        Leader &L = leaders[f];

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
        float precess = ORBIT_PRECESS * (0.4f + 0.2f * (float)f) * delta;
        L.h = Vector3Normalize(Vector3RotateByAxisAngle(L.h, L.p, precess));

        // re-orthonormalize the heading against position to keep numerical drift in check.
        L.h = Vector3Normalize(Vector3Subtract(L.h, Vector3Scale(L.p, Vector3DotProduct(L.p, L.h))));

        projectLeader(L, sphere_radius);
    }
}

void FlockingMovement::updateMembers(float delta)
{
    // separation is within-flock only (members keep distance from their own flockmates).
    for (int i = 0; i < n; i++)
    {
        Member &m = members[i];
        Vector2 leaderPos = leaders.empty() ? fleet_position : leaders[m.flock].screen;

        Vector2 acc = Vector2Scale(Vector2Subtract(leaderPos, m.pos), SEEK_GAIN);

        Vector2 sep = {0.0f, 0.0f};
        for (int j = 0; j < n; j++)
        {
            if (j == i || members[j].flock != m.flock)
            {
                continue;
            }
            Vector2 d = Vector2Subtract(m.pos, members[j].pos);
            float dist2 = d.x * d.x + d.y * d.y;
            if (dist2 < SEP_RADIUS * SEP_RADIUS && dist2 > 1e-4f)
            {
                sep = Vector2Add(sep, Vector2Scale(d, 1.0f / sqrtf(dist2))); // unit push away
            }
        }
        acc = Vector2Add(acc, Vector2Scale(sep, SEP_GAIN));

        m.vel = Vector2Add(m.vel, Vector2Scale(acc, delta));
        m.vel = Vector2Scale(m.vel, 1.0f - std::min(1.0f, MEMBER_DAMPING * delta));
        float sp = Vector2Length(m.vel);
        if (sp > MEMBER_MAXSPEED)
        {
            m.vel = Vector2Scale(m.vel, MEMBER_MAXSPEED / sp);
        }
        m.pos = Vector2Add(m.pos, Vector2Scale(m.vel, delta));
        m.depth = leaders.empty() ? 1.0f : leaders[m.flock].depth;
    }
}

void FlockingMovement::writeOutputs()
{
    for (int i = 0; i < n; i++)
    {
        position[i] = members[i].pos;
        depth[i] = members[i].depth;
    }
}

void FlockingMovement::update(float delta)
{
    if (state == DFS_APPROACHING)
    {
        fleet_position.x += FLEET_SPEED * delta * approach_dir;
        updateLeadersApproach(delta);
        if (approach_dir * (fleet_position.x - engage_x) >= 0.0f)
        {
            state = DFS_ENGAGING;
            engage_timer = 0.0f;
            beginEngage();
        }
    }
    else if (state == DFS_ENGAGING)
    {
        engage_timer += delta;
        float u = std::min(engage_timer / ENGAGE_EXPAND_TIME, 1.0f);
        float ease = u * u * (3.0f - 2.0f * u); // smoothstep
        sphere_radius = base_radius * (1.0f + (ENGAGE_RADIUS_MULT - 1.0f) * ease);
        updateLeadersEngage(delta);
    }
    else // DFS_RETREATING
    {
        fleet_position.x -= FLEET_SPEED * delta * approach_dir;
        for (auto &L : leaders)
        {
            L.screen.x -= FLEET_SPEED * delta * approach_dir;
        }
    }

    updateMembers(delta);
    writeOutputs();
}
} // namespace

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
