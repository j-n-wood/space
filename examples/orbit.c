#include <stdio.h>
#include <math.h>
#include <stdbool.h>

// --- Constants ---
#define MU 39.4784176      // Gravitational parameter (AU^3 / yr^2)
#define DT 0.0005          // Simulation time step (approx 4 hours)
#define MAX_ITER 25        // Bisection iterations

typedef struct { double x, y; } Vec2;

// --- Math Helpers ---
Vec2 add(Vec2 a, Vec2 b) { return (Vec2){a.x + b.x, a.y + b.y}; }
Vec2 sub(Vec2 a, Vec2 b) { return (Vec2){a.x - b.x, a.y - b.y}; }
Vec2 mul(Vec2 a, double s) { return (Vec2){a.x * s, a.y * s}; }
double mag(Vec2 a) { return sqrt(a.x * a.x + a.y * a.y); }
Vec2 norm(Vec2 a) { double m = mag(a); return (m > 0) ? mul(a, 1.0/m) : (Vec2){0,0}; }

// --- Planetary Kinematics ---
// Returns position of a planet in a circular orbit at time t
Vec2 get_planet_pos(double r, double phase, double t) {
    double omega = sqrt(MU / pow(r, 3));
    double angle = phase + (omega * t);
    return (Vec2){r * cos(angle), r * sin(angle)};
}

// Returns velocity of a planet at time t
Vec2 get_planet_vel(double r, double phase, double t) {
    double omega = sqrt(MU / pow(r, 3));
    double v_mag = omega * r;
    double angle = phase + (omega * t);
    // Velocity is perpendicular to position vector
    return (Vec2){-v_mag * sin(angle), v_mag * cos(angle)};
}

// --- The Simulation "Worker" ---
// Simulates a full flight and returns a "score" (0 is perfect intercept)
double evaluate_flight(double T_total, Vec2 s_pos, Vec2 s_vel, double s_accel, 
                       double p_r, double p_phase, double t_now) {
    
    double t_flip = T_total * 0.5; // Simplified flip point
    Vec2 target_arrival_pos = get_planet_pos(p_r, p_phase, t_now + T_total);
    Vec2 target_arrival_vel = get_planet_vel(p_r, p_phase, t_now + T_total);

    for (double t = 0; t < T_total; t += DT) {
        double r_mag = mag(s_pos);
        Vec2 a_grav = mul(norm(s_pos), -MU / (r_mag * r_mag));
        Vec2 a_thrust;

        if (t < t_flip) {
            // PHASE 1: Acceleration - Aim at future intercept
            a_thrust = mul(norm(sub(target_arrival_pos, s_pos)), s_accel);
        } else {
            // PHASE 2: Braking - Aim against relative velocity to match target
            // We want s_vel to eventually equal target_arrival_vel
            Vec2 rel_v = sub(s_vel, target_arrival_vel);
            a_thrust = mul(norm(rel_v), -s_accel);
        }

        Vec2 a_total = add(a_grav, a_thrust);
        s_vel = add(s_vel, mul(a_total, DT));
        s_pos = add(s_pos, mul(s_vel, DT));
    }

    // Cost function: How far are we from target, and how poorly matched is velocity?
    double dist_error = mag(sub(s_pos, target_arrival_pos));
    double vel_error = mag(sub(s_vel, target_arrival_vel));
    
    // We weigh distance heavily for the solver
    return dist_error + (vel_error * 0.2);
}

// --- The Planner ---
// Uses Bisection to find the optimal T_total
double solve_arrival_time(Vec2 s_pos, Vec2 s_vel, double s_accel, 
                          double p_r, double p_phase, double t_now) {
    double t_min = 0.05; // 18 days
    double t_max = 4.0;  // 4 years
    double mid;

    for (int i = 0; i < MAX_ITER; i++) {
        mid = (t_min + t_max) / 2.0;
        
        // Sampling points to see if we should increase or decrease T
        double score_low = evaluate_flight(t_min, s_pos, s_vel, s_accel, p_r, p_phase, t_now);
        double score_mid = evaluate_flight(mid, s_pos, s_vel, s_accel, p_r, p_phase, t_now);

        if (score_mid < score_low) {
            t_min = mid;
        } else {
            t_max = mid;
        }
    }
    return mid;
}

// --- Main Integration Example ---
int main() {
    // Starting State: Earth orbit (1 AU), moving at Earth's orbital speed
    Vec2 ship_pos = {1.0, 0.0};
    Vec2 ship_vel = {0.0, 6.283}; // ~2pi AU/yr
    double engine_a = 20.0;       // High thrust (20 AU/yr^2)

    // Target: Mars (1.524 AU), currently at 45 degrees (0.785 rad)
    double mars_r = 1.524;
    double mars_phase = 0.785;
    double t_now = 0.0;

    printf("--- Orbital Solver Initiated ---\n");
    printf("Ship Start: (%.2f, %.2f) Accel: %.1f AU/yr^2\n", ship_pos.x, ship_pos.y, engine_a);
    printf("Targeting Planet at %.2f AU...\n", mars_r);

    double best_t = solve_arrival_time(ship_pos, ship_vel, engine_a, mars_r, mars_phase, t_now);

    printf("\n--- Result ---\n");
    printf("Estimated Flight Time: %.4f years (%.1f days)\n", best_t, best_t * 365.25);
    printf("Flip Point: %.4f years\n", best_t * 0.5);
    
    return 0;
}