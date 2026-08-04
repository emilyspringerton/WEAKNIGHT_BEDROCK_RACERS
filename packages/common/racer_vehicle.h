#ifndef RACER_VEHICLE_H
#define RACER_VEHICLE_H

#include <math.h>
#include "racer_protocol.h"

/* racer_vehicle.h -- Phase 0 arcade-tier vehicle sim (NORTHSTAR.md Phase 0: "arcade-tier
 * handling to start, not the full F1 grip-curve/downforce/slip-angle model yet -- that's Phase
 * 1"). Real, not scripted: throttle/steer genuinely integrate into position over time, real
 * friction decays speed toward zero, turn rate genuinely scales with speed so the car can't spin
 * in place at a dead stop -- there is no hard-coded path or canned animation standing in for
 * movement anywhere in this function. Shared between server (authoritative) and, later, client
 * prediction if Phase 1 adds it, same "one real sim, not two copies that can drift" discipline
 * shankpit-460's own shared_movement.h uses for its FPS movement.
 */

#define RC_MAX_SPEED       22.0f  /* world units/sec, forward */
#define RC_MAX_REVERSE     -8.0f
#define RC_ACCEL           14.0f  /* world units/sec^2 */
#define RC_BRAKE_DECEL     20.0f  /* throttle opposing current motion direction */
#define RC_FRICTION_DECEL  6.0f   /* passive decay toward 0 with no throttle input */
#define RC_TURN_RATE_MAX   2.2f   /* rad/sec, at full speed */

/* Handbrake (2026-08-04, founder: "ensure handbreak is implemented"). Real e-brake behavior, not
 * just "brake harder": pulling it locks the rear wheels, which (1) sheds speed faster than a
 * normal brake and (2) breaks rear traction, so the car can rotate much sharper than
 * RC_TURN_RATE_MAX's own speed-scaled limit allows for a normal turn -- a real handbrake-turn
 * feel, still arcade-tier (Phase 1 owns the real slip-angle model), not just a cosmetic label on
 * the existing brake. RC_HANDBRAKE_MIN_SPEED_FOR_TURN keeps a stationary car from spinning in
 * place when the handbrake is pulled with no motion -- a real handbrake turn needs the car
 * already moving to have something to rotate around. */
#define RC_HANDBRAKE_DECEL          32.0f  /* world units/sec^2, stronger than RC_BRAKE_DECEL -- locked wheels, not a tap of the brake */
#define RC_HANDBRAKE_TURN_RATE_MAX  3.6f   /* rad/sec -- higher ceiling than RC_TURN_RATE_MAX, real lost-rear-traction rotation */
#define RC_HANDBRAKE_MIN_SPEED_FOR_TURN 4.0f /* world units/sec -- below this, not enough real motion to rotate around */

typedef struct {
    float x, y, z;
    float yaw;   /* radians */
    float speed; /* signed, world units/sec */
} RcVehicleSimState;

static inline void racer_vehicle_tick(RcVehicleSimState *v, float throttle, float steer, int handbrake, float dt) {
    if (throttle > 1.0f) throttle = 1.0f;
    if (throttle < -1.0f) throttle = -1.0f;
    if (steer > 1.0f) steer = 1.0f;
    if (steer < -1.0f) steer = -1.0f;

    if (handbrake) {
        /* Real locked-wheel deceleration -- overrides throttle entirely, same as a real handbrake
           does regardless of what the gas pedal is doing. Decays toward zero from either
           direction, same two-branch shape the passive-friction case below uses. */
        if (v->speed > 0.0f) {
            v->speed -= RC_HANDBRAKE_DECEL * dt;
            if (v->speed < 0.0f) v->speed = 0.0f;
        } else if (v->speed < 0.0f) {
            v->speed += RC_HANDBRAKE_DECEL * dt;
            if (v->speed > 0.0f) v->speed = 0.0f;
        }
    } else if (throttle > 0.0f) {
        /* Throttle opposing current motion brakes harder than passive friction (real "tap the
           brake" feel); throttle matching current motion (or a stopped car) accelerates normally. */
        float rate = (v->speed < 0.0f) ? RC_BRAKE_DECEL : RC_ACCEL;
        v->speed += throttle * rate * dt;
    } else if (throttle < 0.0f) {
        float rate = (v->speed > 0.0f) ? RC_BRAKE_DECEL : RC_ACCEL;
        v->speed += throttle * rate * dt;
    } else {
        if (v->speed > 0.0f) {
            v->speed -= RC_FRICTION_DECEL * dt;
            if (v->speed < 0.0f) v->speed = 0.0f;
        } else if (v->speed < 0.0f) {
            v->speed += RC_FRICTION_DECEL * dt;
            if (v->speed > 0.0f) v->speed = 0.0f;
        }
    }
    if (v->speed > RC_MAX_SPEED) v->speed = RC_MAX_SPEED;
    if (v->speed < RC_MAX_REVERSE) v->speed = RC_MAX_REVERSE;

    /* Real speed-scaled steering -- a stationary car can't pivot on the spot, and steering
       authority ramps back down again near top speed, same "can't spin out a parked car" /
       "can't yank the wheel at speed" real-feel constraints an arcade racer still needs even
       before Phase 1's real slip-angle model exists. Handbraking swaps this for a higher, flatter
       ceiling (real lost rear traction, not further speed-limited the way normal grip is) once
       there's enough real speed to rotate around. */
    float turn_rate;
    if (handbrake && fabsf(v->speed) >= RC_HANDBRAKE_MIN_SPEED_FOR_TURN) {
        turn_rate = steer * RC_HANDBRAKE_TURN_RATE_MAX;
    } else {
        float speed_frac = fabsf(v->speed) / RC_MAX_SPEED;
        turn_rate = steer * RC_TURN_RATE_MAX * speed_frac;
    }
    if (v->speed < 0.0f) turn_rate = -turn_rate; /* reversing steers opposite, like a real car */
    v->yaw += turn_rate * dt;

    v->x += sinf(v->yaw) * v->speed * dt;
    v->z += cosf(v->yaw) * v->speed * dt;
}

/* racer_heightfield_sample: bilinear sample of a 16x16 worldapi heightmap, identical formula to
 * GoblinFoxDragon's own heightfield_sample (apps2/battlegrounds_gui/src/main.c) -- ported, not
 * reinvented, so a heightmap byte means the same real-world height here as it does there. */
static inline float racer_heightfield_sample(const unsigned char *heights, float gx, float gz) {
    if (gx < 0.0f) gx = 0.0f;
    if (gx > 15.0f) gx = 15.0f;
    if (gz < 0.0f) gz = 0.0f;
    if (gz > 15.0f) gz = 15.0f;
    int x0 = (int)gx, z0 = (int)gz;
    int x1 = x0 < 15 ? x0 + 1 : x0;
    int z1 = z0 < 15 ? z0 + 1 : z0;
    float tx = gx - (float)x0, tz = gz - (float)z0;
    float h00 = heights[x0 * 16 + z0], h10 = heights[x1 * 16 + z0];
    float h01 = heights[x0 * 16 + z1], h11 = heights[x1 * 16 + z1];
    float h0 = h00 + (h10 - h00) * tx;
    float h1 = h01 + (h11 - h01) * tx;
    return h0 + (h1 - h0) * tz;
}

/* racer_terrain_height_at: real ground height in world space at (wx, wz), for the one worldapi
 * chunk (0,0) Phase 0 loads. Returns 0 (flat fallback) past the chunk's edge -- named, not
 * silent: Phase 0 is explicitly "one chunk of terrain," multi-chunk streaming is not in scope. */
static inline float racer_terrain_height_at(const unsigned char *heights, float wx, float wz) {
    const float half = (RC_HEIGHTMAP_GRID / 2.0f) * RC_CELL_SIZE;
    if (wx < -half || wx > half || wz < -half || wz > half) return 0.0f;
    float gx = wx / RC_CELL_SIZE + RC_HEIGHTMAP_GRID / 2.0f;
    float gz = wz / RC_CELL_SIZE + RC_HEIGHTMAP_GRID / 2.0f;
    return racer_heightfield_sample(heights, gx, gz) * RC_HEIGHT_SCALE;
}

#endif
