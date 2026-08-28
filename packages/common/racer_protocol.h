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
#define RC_PACKET_REJECT   4 /* real rejection, not a silent drop -- see RcConnectPacket's own doc comment */

/* Connect-ticket auth (2026-08-28, "build login from the beginning take it from GFD") -- direct
 * port of shankpit-460's own real, proven wire format (apps/server/src/main.c's own
 * TICKET_PAYLOAD_LEN/TICKET_MAC_LEN/TICKET_TOTAL_LEN), not a new design. Minted by IDUNA's
 * RacerTicketHandler (internal/http/handlers/racer_ticket.go) from a real player JWT (itself from
 * POST /api/v1/auth/email/login -- GFD's own apps2/battlegrounds_gui login-screen pattern, ported
 * into apps/client below). player_id(16) + expires_at(4, LE u32) + hmac_sha256(secret,
 * player_id||expires_at) truncated to 16 bytes = 36 raw bytes, appended after RcConnectPacket's
 * own header. Both sides must agree on RACER_TICKET_SECRET byte-for-byte (raw string bytes, not
 * hex-decoded -- see racer_ticket.go's own doc comment). */
#define RC_TICKET_PAYLOAD_LEN 20 /* player_id(16) + expires_at(4) */
#define RC_TICKET_MAC_LEN     16 /* truncated HMAC-SHA256 */
#define RC_TICKET_TOTAL_LEN   (RC_TICKET_PAYLOAD_LEN + RC_TICKET_MAC_LEN) /* 36 */

#define RC_WORLDAPI_SCENE 0 /* Meadow -- real gentle rolling terrain, per worldapi's own scene table */
#define RC_HEIGHTMAP_GRID 16
#define RC_CELL_SIZE 8.0f      /* world units per heightmap cell -- 128x128 world footprint for one chunk */
#define RC_HEIGHT_SCALE 1.5f   /* heightmap unit (a block count) -> world-space Y, matches GoblinFoxDragon's own TERRAIN_TEST_HEIGHT_SCALE convention */

typedef struct {
    unsigned char type;
    unsigned char client_id;
    unsigned int sequence;
} RcHeader;

/* RcConnectPacket now carries a real ticket, not just a bare header -- an anonymous, ticketless
 * CONNECT (Phase 0's own original shape) is no longer accepted once RACER_TICKET_SECRET is
 * configured server-side (fail closed, matching shankpit-460's own verify_connect_ticket
 * discipline: an unset secret rejects everything rather than silently accepting unauthenticated
 * connects). */
typedef struct {
    RcHeader hdr;
    unsigned char ticket[RC_TICKET_TOTAL_LEN];
} RcConnectPacket;

typedef struct {
    RcHeader hdr;
    unsigned char client_id;
} RcWelcomePacket;

/* RcRejectPacket -- sent once, unreliably (UDP, no retry), when a CONNECT's ticket fails
 * verification (bad signature, expired, or no secret configured server-side) or when
 * RC_MAX_VEHICLES has no free human slot. reason is a short human-readable string the client
 * shows directly, not an error code -- this repo is small enough that a real client and a real
 * server are always built from the same source tree, so wire-stability across independent
 * client/server versions isn't a real constraint the way it is for shankpit-460's own
 * multi-version fleet. */
#define RC_REJECT_REASON_MAX 63
typedef struct {
    RcHeader hdr;
    char reason[RC_REJECT_REASON_MAX + 1];
} RcRejectPacket;

#define RC_BTN_HANDBRAKE 1

typedef struct {
    RcHeader hdr;
    unsigned int cmd_sequence;
    unsigned int cmd_time_ms;
    float throttle; /* -1..1, real analog even though keyboard only drives -1/0/1 today */
    float steer;    /* -1..1 */
    unsigned int buttons; /* RC_BTN_* bitmask -- handbrake today, room for more without a wire break */
} RcUserCmdPacket;

/* Two real vehicle classes (2026-08-28 pivot -- docs/NORTHSTAR.md PIVOT section, "its like 2
 * different gears in a car almost and the driving is crisp"): RC_VEH_BUGGY is the existing
 * flat-rate Phase 0 model (packages/common/racer_vehicle.h's own racer_vehicle_tick);
 * RC_VEH_BIKE is the new 5-speed-gearbox model (racer_bike_tick), its gear selection decided by
 * a real PARENA-compiled function. gear is only meaningful when vehicle_type == RC_VEH_BIKE
 * (always 0 for a buggy) -- sent in every snapshot so the client can show it on the HUD, a real
 * player-visible signal for "the shifting matters," not just an internal server detail. */
#define RC_VEH_BUGGY 0
#define RC_VEH_BIKE  1

typedef struct {
    float x, y, z;
    float yaw;   /* radians, world-space heading */
    float speed; /* world units/sec, signed (negative = reversing) */
    unsigned char vehicle_type; /* RC_VEH_* */
    unsigned char gear;         /* 1..RC_BIKE_GEARS for RC_VEH_BIKE, 0 otherwise */
} RcVehicleState;

/* RC_MAX_VEHICLES (2026-08-04, founder: "can we get 8 player online bot matches? same pattern as
 * before 7 bots so i can queue into a game") -- same real shape shankpit-460's own bot pool used
 * (a fixed slot count, bots filling whatever a human doesn't claim, direct-connect rather than a
 * separate matchmaker service for now, "bring the lobby back once bot matches work" was that
 * fork's own phrasing and the same simplification applies here). Slot 0 is reserved for the first
 * real human to CONNECT; slots 1-7 are always-active bots. */
#define RC_MAX_VEHICLES 8

typedef struct {
    RcHeader hdr;
    unsigned int server_tick;
    unsigned char active[RC_MAX_VEHICLES];  /* 0 = no vehicle in this slot (human slot, unclaimed) */
    unsigned char is_bot[RC_MAX_VEHICLES];
    RcVehicleState vehicles[RC_MAX_VEHICLES];
} RcSnapshotPacket;

#endif
