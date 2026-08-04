#ifndef RACER_PROTOCOL_H
#define RACER_PROTOCOL_H

/* racer_protocol.h -- Phase 0 wire protocol (WEAKNIGHT_BEDROCK_RACERS/docs/NORTHSTAR.md).
 *
 * Deliberately NOT shankpit-460's own protocol.h -- that struct carries weapons, projectiles,
 * health/shield, KO state, none of which Phase 0 (or the F1-tier vehicle model Phase 1 actually
 * needs) has any use for. What IS reused from shankpit-460 is the *shape* of the pattern this
 * mirrors: a small NetHeader-style framing struct, a fixed-tick server-authoritative sim, and a
 * UserCmd-in/Snapshot-out packet pair -- just sized for one vehicle's real state instead of a
 * full FPS entity.
 *
 * RC_CELL_SIZE/RC_HEIGHT_SCALE are shared, not duplicated per-file, because server and client
 * both convert the same worldapi heightmap bytes into world-space Y -- two independent copies of
 * this math WOULD drift (the exact class of bug GoblinFoxDragon's own dfzone_height_at/
 * build_heightfield_mesh doc comments already name as a real risk), and here it's worse: drift
 * between client and server here means the client renders the car sitting on terrain the server
 * doesn't agree with.
 */

#define RC_PACKET_CONNECT  0
#define RC_PACKET_WELCOME  1
#define RC_PACKET_USERCMD  2
#define RC_PACKET_SNAPSHOT 3

#define RC_WORLDAPI_SCENE 0 /* Meadow -- real gentle rolling terrain, per worldapi's own scene table */
#define RC_HEIGHTMAP_GRID 16
#define RC_CELL_SIZE 8.0f      /* world units per heightmap cell -- 128x128 world footprint for one chunk */
#define RC_HEIGHT_SCALE 1.5f   /* heightmap unit (a block count) -> world-space Y, matches GoblinFoxDragon's own TERRAIN_TEST_HEIGHT_SCALE convention */

typedef struct {
    unsigned char type;
    unsigned char client_id;
    unsigned int sequence;
} RcHeader;

typedef struct {
    RcHeader hdr;
} RcConnectPacket;

typedef struct {
    RcHeader hdr;
    unsigned char client_id;
} RcWelcomePacket;

typedef struct {
    RcHeader hdr;
    unsigned int cmd_sequence;
    unsigned int cmd_time_ms;
    float throttle; /* -1..1, real analog even though keyboard only drives -1/0/1 today */
    float steer;    /* -1..1 */
} RcUserCmdPacket;

typedef struct {
    float x, y, z;
    float yaw;   /* radians, world-space heading */
    float speed; /* world units/sec, signed (negative = reversing) */
} RcVehicleState;

typedef struct {
    RcHeader hdr;
    unsigned int server_tick;
    RcVehicleState vehicle;
} RcSnapshotPacket;

#endif
