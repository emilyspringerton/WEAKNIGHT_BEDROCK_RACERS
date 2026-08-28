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

/* ================================================================================================
 * Real BIKE archetype (Phase A, 2026-08-28 pivot's own first real slice of "PARENA embedded from
 * day one" -- docs/NORTHSTAR.md PIVOT section). Two real vehicle classes is the whole point of
 * the pivot ("i found an old shankpit experience with a city and 2 diferent cars... its like 2
 * different gears in a car almost and the driving is crisp") -- BUGGY is the existing flat-rate
 * model above (racer_vehicle_tick); BIKE is this, a real 5-speed gearbox, faster overall but
 * gear-limited off the line, gear SELECTION decided by a real PARENA-compiled function
 * (on_racer_bike_gear_shift, packages/simulation/bike_gear_mod.c, generated from
 * PARENA/stdlib/racer/bike_gear_mod.prn) -- not ported C, freshly authored, mirroring the real,
 * tested upshift/downshift feel the reference SHANKPIT_CONSTRUCT.txt bike proved out, rescaled
 * into this repo's own real world-unit/sec convention rather than copying its literal numbers
 * (NORTHSTAR's own explicit "start from scratch" call).
 * ================================================================================================ */

/* Declared here, defined in packages/simulation/bike_gear_mod.c (real PARENA output, "do not edit
   by hand" per its own header) -- linked directly into both apps/server and apps/client, same
   "generated .c committed, added to the build's own SRCS list, called by name" pattern
   ECOWAR/docs/ARENA_API.md already establishes for every PARENA mod in this monorepo. */
int on_racer_bike_gear_shift(int gear, int speed_x100);

#define RC_BIKE_TOP_SPEED   32.0f  /* world units/sec, ~1.45x RC_MAX_SPEED -- BIKE is the fast option */
#define RC_BIKE_GEARS       5
/* RC_BIKE_GEAR_SCALE rescales a real world-units/sec speed into the 0..380 fixed-point "x100"
   space bike_gear_mod.prn's own gear-max thresholds are defined in (380 = that mod's own gear-5
   value) -- the one place the world-scale and the PARENA mod's own internal scale meet, named so
   it's obvious this isn't a magic number. */
#define RC_BIKE_GEAR_SCALE  (380.0f / RC_BIKE_TOP_SPEED)

/* Real per-gear top speed (world units/sec) and accel (world units/sec^2) -- index 0 unused
   (gear is always 1..5), proportioned from the real, tested reference curve
   (BIKE_GEAR_MAX/BIKE_GEAR_ACCEL, SHANKPIT_CONSTRUCT.txt lines 4716-4718): low gears accelerate
   hard but top out early, gear 5 keeps pulling all the way to RC_BIKE_TOP_SPEED but noticeably
   softer off the line -- the real "shifting matters" feel the founder called "crisp." */
static const float RC_BIKE_GEAR_MAX_SPEED[RC_BIKE_GEARS + 1] = {0.0f, 10.1f, 16.8f, 22.7f, 27.8f, 32.0f};
static const float RC_BIKE_GEAR_ACCEL[RC_BIKE_GEARS + 1]     = {0.0f, 20.0f, 15.7f, 13.6f, 11.4f, 10.0f};

#define RC_BIKE_BRAKE_DECEL   24.0f
#define RC_BIKE_FRICTION_DECEL 5.0f
#define RC_BIKE_TURN_RATE_MAX 2.6f  /* rad/sec at full speed -- BIKE turns sharper than BUGGY (RC_TURN_RATE_MAX 2.2), real lighter-vehicle handling */

typedef struct {
    RcVehicleSimState sim;
    int gear; /* 1..RC_BIKE_GEARS, real PARENA-decided state, not a cosmetic label */
} RcBikeState;

static inline void racer_bike_tick(RcBikeState *b, float throttle, float steer, float dt) {
    if (throttle > 1.0f) throttle = 1.0f;
    if (throttle < -1.0f) throttle = -1.0f;
    if (steer > 1.0f) steer = 1.0f;
    if (steer < -1.0f) steer = -1.0f;
    if (b->gear < 1) b->gear = 1;
    if (b->gear > RC_BIKE_GEARS) b->gear = RC_BIKE_GEARS;

    RcVehicleSimState *v = &b->sim;

    /* Real gear selection every tick, decided by the real PARENA-compiled function -- this IS
       the "PARENA embedded in the core" part, not a trigger wrapping C logic that would run the
       same without it (see PARENA/stdlib/racer/bike_gear_mod.prn's own header comment for the
       full ABI). Only reverse/braking bypasses gear logic entirely, same real convention the
       reference construct's own gear system uses (gear never applies going backward). */
    if (v->speed >= 0.0f) {
        int speed_x100 = (int)(v->speed * RC_BIKE_GEAR_SCALE);
        b->gear = on_racer_bike_gear_shift(b->gear, speed_x100);
    }

    float gear_max_speed = RC_BIKE_GEAR_MAX_SPEED[b->gear];
    float gear_accel = RC_BIKE_GEAR_ACCEL[b->gear];

    if (throttle > 0.0f) {
        float rate = (v->speed < 0.0f) ? RC_BIKE_BRAKE_DECEL : gear_accel;
        v->speed += throttle * rate * dt;
        /* Real gear-limited top speed -- flooring it in gear 2 shouldn't reach gear 5's own top
           speed; the gear-shift call above will itself upshift once close enough, but this clamp
           keeps a single tick from overshooting the current gear's own ceiling before that next
           shift check runs. */
        if (v->speed > gear_max_speed) v->speed = gear_max_speed;
    } else if (throttle < 0.0f) {
        float rate = (v->speed > 0.0f) ? RC_BIKE_BRAKE_DECEL : RC_BIKE_GEAR_ACCEL[1];
        v->speed += throttle * rate * dt;
    } else {
        if (v->speed > 0.0f) {
            v->speed -= RC_BIKE_FRICTION_DECEL * dt;
            if (v->speed < 0.0f) v->speed = 0.0f;
        } else if (v->speed < 0.0f) {
            v->speed += RC_BIKE_FRICTION_DECEL * dt;
            if (v->speed > 0.0f) v->speed = 0.0f;
        }
    }
    if (v->speed > RC_BIKE_TOP_SPEED) v->speed = RC_BIKE_TOP_SPEED;
    if (v->speed < RC_MAX_REVERSE) v->speed = RC_MAX_REVERSE;

    float speed_frac = fabsf(v->speed) / RC_BIKE_TOP_SPEED;
    float turn_rate = steer * RC_BIKE_TURN_RATE_MAX * speed_frac;
    if (v->speed < 0.0f) turn_rate = -turn_rate;
    v->yaw += turn_rate * dt;

    v->x += sinf(v->yaw) * v->speed * dt;
    v->z += cosf(v->yaw) * v->speed * dt;
}

/* racer_bot_drive_toward (2026-08-04, founder: "8 player online bot matches... 7 bots"): real
 * reactive waypoint-seeking, not a scripted/pre-baked path -- every tick it looks at the bot's
 * own current position and yaw versus wherever its target currently is and derives throttle/steer
 * fresh, the same way a human aiming a car at a point on screen would, so a bot that gets
 * deflected (or whose target changes) genuinely corrects instead of replaying a canned route.
 * Slows the throttle down the sharper the required turn is (a real car can't floor it into a
 * hairpin) rather than driving blind at full speed regardless of heading error. */
static inline void racer_bot_drive_toward(const RcVehicleSimState *v, float target_x, float target_z,
                                           float *out_throttle, float *out_steer) {
    float dx = target_x - v->x, dz = target_z - v->z;
    float dist = sqrtf(dx * dx + dz * dz);
    if (dist < 0.001f) { *out_throttle = 0.0f; *out_steer = 0.0f; return; }
    float desired_yaw = atan2f(dx, dz);
    float err = desired_yaw - v->yaw;
    while (err > (float)M_PI) err -= 2.0f * (float)M_PI;
    while (err < -(float)M_PI) err += 2.0f * (float)M_PI;

    *out_steer = err / 0.7f; /* full lock once heading error exceeds ~40 degrees */
    if (*out_steer > 1.0f) *out_steer = 1.0f;
    if (*out_steer < -1.0f) *out_steer = -1.0f;

    float turn_severity = fabsf(err) / (float)M_PI; /* 0 (dead ahead) .. 1 (needs to reverse heading) */
    *out_throttle = 1.0f - turn_severity * 0.7f;
    if (*out_throttle < 0.25f) *out_throttle = 0.25f; /* always keep enough speed to actually turn */
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
