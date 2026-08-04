#ifndef RACER_TRACK_STADIUM_H
#define RACER_TRACK_STADIUM_H

/* racer_track_stadium.h -- a real port of SHANKPIT's own coliseum arena (packages/common/
 * physics.h: map_geo_stadium / init_stadium_terrain / stadium_rally_loop), founder: "ther eis a
 * shankpit map that is like a big arena with like colleseum seats and there is like a dirt track
 * around it" -> "we can port that map" -> "from og shankpit engine".
 *
 * A SECOND, SELF-CONTAINED TRACK, NOT A SECOND VOXEL ENGINE: WEAKNIGHT_BEDROCK_RACERS' own
 * CLAUDE.md says "do not build a second, parallel voxel engine" for the real, persistent,
 * player-editable GoblinFoxDragon voxel world Meadow already comes from (server/worldapi). That
 * constraint is about not reinventing GENERAL-purpose terrain infrastructure -- it doesn't cover
 * one hardcoded, deterministic height function for one fixed alternate circuit, which is exactly
 * what SHANKPIT's own init_stadium_terrain already is (pure math, no chunk storage, no voxel
 * editing). This file ports that same real formula verbatim (ring/noise/ridge/bowl/berm terms +
 * the real dirt-track carve driven by stadium_rally_loop's own racing line), operating on a plain
 * float array instead of SHANKPIT's own TerrainHeightfield struct -- same numbers, no new engine.
 *
 * Selected via `--track stadium` on both server and client (see their own doc comments) instead
 * of a wire-protocol track-id field -- both sides already have to agree on --worldapi-host/
 * --server-host by hand today, so this follows the same existing convention rather than adding a
 * new one. A real, named limitation: launching server and client with mismatched --track values
 * produces disagreeing terrain with no error -- acceptable for this first port pass, not silently
 * pretending it can't happen.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Matches SHANKPIT's own STADIUM_TERRAIN_W/H/CELL exactly (packages/common/physics.h) -- a
 * faithful port keeps the same real-world scale, not a shrunk approximation. */
#define RC_STADIUM_GRID 140
#define RC_STADIUM_CELL 14.0f
#define RC_STADIUM_ORIGIN (-(RC_STADIUM_GRID * RC_STADIUM_CELL * 0.5f))
#define RC_STADIUM_HALF_BOUNDS (RC_STADIUM_GRID * RC_STADIUM_CELL * 0.5f)

typedef struct { float x, z; } RcStadiumVec2;

/* Real racing line, ported verbatim from SHANKPIT's stadium_rally_loop -- the dirt track ring
 * the founder described ("a dirt track around it"). */
static const RcStadiumVec2 rc_stadium_rally_loop[] = {
    {680.0f, -180.0f},
    {620.0f, 250.0f},
    {360.0f, 610.0f},
    {-90.0f, 700.0f},
    {-430.0f, 560.0f},
    {-690.0f, 250.0f},
    {-720.0f, -210.0f},
    {-500.0f, -600.0f},
    {-120.0f, -730.0f},
    {300.0f, -640.0f},
    {620.0f, -420.0f}
};
#define RC_STADIUM_RALLY_LOOP_COUNT (int)(sizeof(rc_stadium_rally_loop) / sizeof(rc_stadium_rally_loop[0]))

static inline float rc_stadium_hash_noise(float x, float z) {
    return sinf(x * 0.00431f + z * 0.00711f) * cosf(z * 0.00377f - x * 0.00623f);
}

static inline float rc_stadium_point_to_segment_dist(float px, float pz, float ax, float az, float bx, float bz) {
    float vx = bx - ax, vz = bz - az;
    float wx = px - ax, wz = pz - az;
    float vv = vx * vx + vz * vz;
    float t = (vv > 0.0001f) ? ((wx * vx + wz * vz) / vv) : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float cx = ax + vx * t, cz = az + vz * t;
    float dx = px - cx, dz = pz - cz;
    return sqrtf(dx * dx + dz * dz);
}

static inline float rc_stadium_road_distance(float x, float z) {
    float best = 1e9f;
    for (int i = 0; i < RC_STADIUM_RALLY_LOOP_COUNT; i++) {
        int next = (i + 1) % RC_STADIUM_RALLY_LOOP_COUNT;
        float d = rc_stadium_point_to_segment_dist(
            x, z,
            rc_stadium_rally_loop[i].x, rc_stadium_rally_loop[i].z,
            rc_stadium_rally_loop[next].x, rc_stadium_rally_loop[next].z
        );
        if (d < best) best = d;
    }
    return best;
}

static inline float rc_stadium_track_weight_at(float x, float z) {
    const float road_half_width = 64.0f, shoulder_width = 86.0f;
    float d = rc_stadium_road_distance(x, z);
    if (d >= shoulder_width) return 0.0f;
    float t = 1.0f - (d - road_half_width) / (shoulder_width - road_half_width);
    if (d <= road_half_width) t = 1.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

/* rc_stadium_stamp/smooth: same real algorithms as SHANKPIT's vox_terrain_stamp/vox_terrain_smooth
 * (physics.h), ported to operate on a flat row-major float[] instead of a TerrainHeightfield*. */
static inline void rc_stadium_stamp(float *h, float cx, float cz, float radius, float target_h, float blend) {
    if (radius <= 0.0f) return;
    int gx0 = (int)((cx - radius - RC_STADIUM_ORIGIN) / RC_STADIUM_CELL); if (gx0 < 0) gx0 = 0;
    int gz0 = (int)((cz - radius - RC_STADIUM_ORIGIN) / RC_STADIUM_CELL); if (gz0 < 0) gz0 = 0;
    int gx1 = (int)((cx + radius - RC_STADIUM_ORIGIN) / RC_STADIUM_CELL); if (gx1 > RC_STADIUM_GRID - 1) gx1 = RC_STADIUM_GRID - 1;
    int gz1 = (int)((cz + radius - RC_STADIUM_ORIGIN) / RC_STADIUM_CELL); if (gz1 > RC_STADIUM_GRID - 1) gz1 = RC_STADIUM_GRID - 1;
    for (int gz = gz0; gz <= gz1; gz++) {
        for (int gx = gx0; gx <= gx1; gx++) {
            float wx = RC_STADIUM_ORIGIN + gx * RC_STADIUM_CELL;
            float wz = RC_STADIUM_ORIGIN + gz * RC_STADIUM_CELL;
            float dx = wx - cx, dz = wz - cz;
            float dist = sqrtf(dx * dx + dz * dz);
            if (dist > radius) continue;
            float falloff = 1.0f - (dist / radius);
            falloff = falloff * falloff * (3.0f - 2.0f * falloff);
            float *cell = &h[gz * RC_STADIUM_GRID + gx];
            *cell = *cell + (target_h - *cell) * blend * falloff;
        }
    }
}

static inline void rc_stadium_smooth(float *h, int passes, float alpha) {
    int n = RC_STADIUM_GRID * RC_STADIUM_GRID;
    float *scratch = (float *)malloc(sizeof(float) * (size_t)n);
    if (!scratch) return;
    for (int pass = 0; pass < passes; pass++) {
        for (int gz = 0; gz < RC_STADIUM_GRID; gz++) {
            for (int gx = 0; gx < RC_STADIUM_GRID; gx++) {
                float sum = 0.0f;
                int cnt = 0;
                for (int dz = -1; dz <= 1; dz++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int sx = gx + dx; if (sx < 0) sx = 0; if (sx > RC_STADIUM_GRID - 1) sx = RC_STADIUM_GRID - 1;
                        int sz = gz + dz; if (sz < 0) sz = 0; if (sz > RC_STADIUM_GRID - 1) sz = RC_STADIUM_GRID - 1;
                        sum += h[sz * RC_STADIUM_GRID + sx];
                        cnt++;
                    }
                }
                float cur = h[gz * RC_STADIUM_GRID + gx];
                float avg = sum / (float)cnt;
                scratch[gz * RC_STADIUM_GRID + gx] = cur + (avg - cur) * alpha;
            }
        }
        memcpy(h, scratch, sizeof(float) * (size_t)n);
    }
    free(scratch);
}

/* rc_stadium_generate_heights: fills out_bytes[RC_STADIUM_GRID*RC_STADIUM_GRID] with the real
 * stadium heightfield, quantized to RC_HEIGHT_SCALE units the exact same way the worldapi
 * heightmap already is (racer_terrain_height_at's own byte*RC_HEIGHT_SCALE convention) -- so the
 * rest of the client/server terrain-height code needs no changes to consume it, just a different
 * grid/cell size. Deterministic (no rand()) -- both server and client independently produce byte-
 * identical terrain, same "no separate/guessed client-side generator" bar the worldapi path
 * already holds itself to. */
static inline void rc_stadium_generate_heights(unsigned char *out_bytes, float rc_height_scale) {
    int n = RC_STADIUM_GRID * RC_STADIUM_GRID;
    float *h = (float *)malloc(sizeof(float) * (size_t)n);
    if (!h) return;

    for (int gz = 0; gz < RC_STADIUM_GRID; gz++) {
        for (int gx = 0; gx < RC_STADIUM_GRID; gx++) {
            float wx = RC_STADIUM_ORIGIN + gx * RC_STADIUM_CELL;
            float wz = RC_STADIUM_ORIGIN + gz * RC_STADIUM_CELL;

            float r = sqrtf(wx * wx + wz * wz);
            float ring = fminf(1.0f, fmaxf(0.0f, (r - 320.0f) / 600.0f));
            float ht = 7.5f + ring * 20.0f;

            ht += sinf(wx * 0.0052f) * 2.3f;
            ht += cosf(wz * 0.0059f) * 2.1f;
            ht += sinf((wx - wz) * 0.0083f) * 1.4f;

            float north_ridge = expf(-((wx + 140.0f) * (wx + 140.0f)) / (2.0f * 360.0f * 360.0f))
                              * expf(-((wz - 580.0f) * (wz - 580.0f)) / (2.0f * 210.0f * 210.0f));
            float west_bowl = expf(-((wx + 650.0f) * (wx + 650.0f)) / (2.0f * 260.0f * 260.0f))
                            * expf(-((wz + 40.0f) * (wz + 40.0f)) / (2.0f * 340.0f * 340.0f));
            float southeast_berm = expf(-((wx - 550.0f) * (wx - 550.0f)) / (2.0f * 300.0f * 300.0f))
                                 * expf(-((wz + 520.0f) * (wz + 520.0f)) / (2.0f * 250.0f * 250.0f));
            ht += north_ridge * 22.0f;
            ht -= west_bowl * 12.0f;
            ht += southeast_berm * 14.0f;

            float core_flat = expf(-(wx * wx) / (2.0f * 300.0f * 300.0f)) * expf(-(wz * wz) / (2.0f * 300.0f * 300.0f));
            ht = ht * (1.0f - core_flat) + 3.0f * core_flat;

            float road = rc_stadium_track_weight_at(wx, wz);
            if (road > 0.0f) {
                float road_profile = 5.5f + 1.4f * sinf(wx * 0.004f + wz * 0.003f);
                ht = ht * (1.0f - road * 0.88f) + road_profile * road * 0.88f;
                float d = rc_stadium_road_distance(wx, wz);
                float shoulder = 1.0f - fminf(1.0f, fabsf(d - 64.0f) / 28.0f);
                if (shoulder > 0.0f) ht += shoulder * 3.4f;
            }

            ht += rc_stadium_hash_noise(wx * 0.7f, wz * 0.7f) * 1.0f;
            h[gz * RC_STADIUM_GRID + gx] = ht;
        }
    }

    /* Same 4 real stamps + smooth pass init_stadium_terrain itself applies: keep the readable
       center flat, raise selected outer sections for rally flow. */
    rc_stadium_stamp(h, 0.0f, 0.0f, 325.0f, 3.0f, 1.0f);
    rc_stadium_stamp(h, -120.0f, 620.0f, 210.0f, 36.0f, 0.8f);
    rc_stadium_stamp(h, 580.0f, -520.0f, 240.0f, 26.0f, 0.7f);
    rc_stadium_stamp(h, -680.0f, 220.0f, 220.0f, 18.0f, 0.7f);
    rc_stadium_smooth(h, 3, 0.44f);

    for (int i = 0; i < n; i++) {
        float b = h[i] / rc_height_scale;
        if (b < 0.0f) b = 0.0f;
        if (b > 255.0f) b = 255.0f;
        out_bytes[i] = (unsigned char)(b + 0.5f);
    }
    free(h);
}

/* rc_stadium_height_at: real ground height in world space at (wx, wz) -- same bilinear-sample +
 * RC_HEIGHT_SCALE convention as racer_heightfield_sample/racer_terrain_height_at, generalized to
 * RC_STADIUM_GRID/RC_STADIUM_CELL instead of the Meadow path's fixed 16x16. Flat (0.0) fallback
 * past the grid edge, same "named, not silent" Phase-0 bar the Meadow path already holds. */
static inline float rc_stadium_height_at(const unsigned char *heights, float wx, float wz, float rc_height_scale) {
    if (wx < -RC_STADIUM_HALF_BOUNDS || wx > RC_STADIUM_HALF_BOUNDS ||
        wz < -RC_STADIUM_HALF_BOUNDS || wz > RC_STADIUM_HALF_BOUNDS) return 0.0f;
    float gx = (wx - RC_STADIUM_ORIGIN) / RC_STADIUM_CELL;
    float gz = (wz - RC_STADIUM_ORIGIN) / RC_STADIUM_CELL;
    if (gx < 0.0f) gx = 0.0f;
    if (gx > (float)(RC_STADIUM_GRID - 1)) gx = (float)(RC_STADIUM_GRID - 1);
    if (gz < 0.0f) gz = 0.0f;
    if (gz > (float)(RC_STADIUM_GRID - 1)) gz = (float)(RC_STADIUM_GRID - 1);
    int x0 = (int)gx, z0 = (int)gz;
    int x1 = x0 < RC_STADIUM_GRID - 1 ? x0 + 1 : x0;
    int z1 = z0 < RC_STADIUM_GRID - 1 ? z0 + 1 : z0;
    float tx = gx - (float)x0, tz = gz - (float)z0;
    float h00 = heights[z0 * RC_STADIUM_GRID + x0], h10 = heights[z0 * RC_STADIUM_GRID + x1];
    float h01 = heights[z1 * RC_STADIUM_GRID + x0], h11 = heights[z1 * RC_STADIUM_GRID + x1];
    float h0 = h00 + (h10 - h00) * tx;
    float h1 = h01 + (h11 - h01) * tx;
    return (h0 + (h1 - h0) * tz) * rc_height_scale;
}

#endif
