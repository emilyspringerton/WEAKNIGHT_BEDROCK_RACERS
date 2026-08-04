/* WEAKNIGHT_BEDROCK_RACERS -- Phase 0 server (docs/NORTHSTAR.md).
 *
 * "Phase 0 is done when: a real player can drive a real vehicle around a real chunk of voxel
 * terrain, server-authoritative, at a stable frame rate, with no fake/scripted movement standing
 * in for real physics." This is that server: a fixed-tick UDP loop, real position/velocity
 * resolved here (never trusts the client's own claimed position), matching shankpit-460's own
 * server-authoritative pattern.
 *
 * Terrain is fetched once at startup from GoblinFoxDragon's own real, already-deployed worldapi
 * HTTP endpoint (GET /heightmap on port 7070) -- reused, not reinvented, per NORTHSTAR.md's own
 * "real infrastructure already in this monorepo" decision. Every vehicle's Y is set from that
 * real heightmap every tick, not held at a constant.
 *
 * 8-slot bot match (2026-08-04, founder: "can we get 8 player online bot matches? same pattern as
 * before 7 bots so i can queue into a game") -- same real shape shankpit-460's own bot pool used:
 * a fixed slot count, the client boots straight into a running match rather than a separate
 * matchmaker/lobby service (that fork's own "bring the lobby back once bot matches work" framing
 * applies here too -- direct-connect now, a real queue/lobby is later work, not faked here). Slot
 * 0 is reserved for the first real human to CONNECT; slots 1..RC_MAX_VEHICLES-1 are always-active
 * bots, each independently, reactively steering toward its own randomized waypoint via
 * racer_bot_drive_toward every tick -- not a pre-baked path.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "../../../packages/common/http_client.h"
#include "../../../packages/common/racer_protocol.h"
#include "../../../packages/common/racer_vehicle.h"

#define RC_SERVER_PORT 7788
#define RC_TICK_HZ 60
#define RC_TICK_DT (1.0f / (float)RC_TICK_HZ)
#define RC_USERCMD_STALE_MS 300 /* real safety net -- a hung/disconnected client must not leave a vehicle stuck at max throttle forever */
#define RC_BOT_RETARGET_MIN_MS 3000
#define RC_BOT_RETARGET_MAX_MS 7000
#define RC_BOT_ARRIVE_DIST 6.0f /* close enough to a waypoint to pick a new one early */

static unsigned char g_heights[256];

typedef struct {
    int active;      /* a vehicle actually exists in this slot */
    int is_bot;
    RcVehicleSimState sim;

    /* Human-only fields */
    struct sockaddr_in addr;
    socklen_t addr_len;
    float latest_throttle, latest_steer;
    unsigned int latest_buttons;
    unsigned int latest_cmd_seq;
    unsigned int last_usercmd_ms;

    /* Bot-only fields */
    float bot_target_x, bot_target_z;
    unsigned int bot_next_retarget_ms;
} VehicleSlot;

static VehicleSlot g_slots[RC_MAX_VEHICLES];

/* fetch_heightmap: same real GET /heightmap?scene=&cx=&cz= call GoblinFoxDragon's own
 * battlegrounds_gui client already makes to this exact worldapi service -- one real HTTP round
 * trip at startup, not a fake/local terrain generator. */
static int fetch_heightmap(const char *worldapi_host, int worldapi_port) {
    char path[64];
    snprintf(path, sizeof(path), "/heightmap?scene=%d&cx=0&cz=0", RC_WORLDAPI_SCENE);
    char resp[8192];
    int status = 0;
    if (http_get_json(worldapi_host, worldapi_port, path, NULL, resp, sizeof(resp), &status) != 0) {
        fprintf(stderr, "fetch_heightmap: worldapi unreachable at %s:%d\n", worldapi_host, worldapi_port);
        return 0;
    }
    if (status != 200) {
        fprintf(stderr, "fetch_heightmap: worldapi returned status %d\n", status);
        return 0;
    }
    size_t found = 0;
    if (!http_extract_json_uint8_array_field(resp, "height", g_heights, 256, &found) || found != 256) {
        fprintf(stderr, "fetch_heightmap: bad heightmap response (found=%zu)\n", found);
        return 0;
    }
    return 1;
}

static unsigned int now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* pick_bot_waypoint: a real random point safely inside the one real terrain chunk Phase 0 loads
 * (see racer_terrain_height_at's own "one chunk for now" doc comment) -- margin keeps bots from
 * constantly aiming at the chunk's own hard edge, where racer_terrain_height_at's flat fallback
 * would otherwise make the ground visibly disagree with the rest of the drive. */
static void pick_bot_waypoint(VehicleSlot *slot, unsigned int now) {
    const float margin = 12.0f;
    const float half = (RC_HEIGHTMAP_GRID / 2.0f) * RC_CELL_SIZE - margin;
    slot->bot_target_x = ((float)(rand() % 2001) / 1000.0f - 1.0f) * half;
    slot->bot_target_z = ((float)(rand() % 2001) / 1000.0f - 1.0f) * half;
    unsigned int span = RC_BOT_RETARGET_MAX_MS - RC_BOT_RETARGET_MIN_MS;
    slot->bot_next_retarget_ms = now + RC_BOT_RETARGET_MIN_MS + (span ? (unsigned int)(rand() % span) : 0);
}

/* spawn_formation: spreads all RC_MAX_VEHICLES slots around a circle at match start so nobody
 * begins stacked on top of another car -- real starting positions, not all zeroed to the origin. */
static void spawn_formation(int slot_index) {
    float angle = (float)slot_index / (float)RC_MAX_VEHICLES * 2.0f * (float)M_PI;
    float radius = 14.0f;
    VehicleSlot *s = &g_slots[slot_index];
    memset(&s->sim, 0, sizeof(s->sim));
    s->sim.x = sinf(angle) * radius;
    s->sim.z = cosf(angle) * radius;
    s->sim.yaw = angle + (float)M_PI; /* face outward from center, arbitrary but not all-identical */
    s->sim.y = racer_terrain_height_at(g_heights, s->sim.x, s->sim.z);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0); /* real log lines when stdout is redirected (systemd, a log file), not just an interactive tty */
    const char *worldapi_host = "localhost";
    int worldapi_port = 7070;
    int server_port = RC_SERVER_PORT;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--worldapi-host") == 0 && i + 1 < argc) worldapi_host = argv[++i];
        else if (strcmp(argv[i], "--worldapi-port") == 0 && i + 1 < argc) worldapi_port = atoi(argv[++i]);
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) server_port = atoi(argv[++i]);
    }

    printf("WEAKNIGHT_BEDROCK_RACERS server (Phase 0, %d-slot bot match) -- fetching real terrain from worldapi %s:%d...\n",
           RC_MAX_VEHICLES, worldapi_host, worldapi_port);
    if (!fetch_heightmap(worldapi_host, worldapi_port)) {
        fprintf(stderr, "FATAL: could not load real terrain from worldapi -- refusing to run on fake/flat ground.\n");
        return 1;
    }
    printf("Real Meadow heightmap loaded (256 columns, scene=%d).\n", RC_WORLDAPI_SCENE);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((unsigned short)server_port);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("bind"); return 1;
    }
    /* Non-blocking recv -- the fixed-tick loop below must never stall waiting on a packet that
       may never arrive (same reasoning shankpit-460's own server loop uses for its recvfrom). */
    int fl = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, fl | O_NONBLOCK);

    printf("Listening on UDP :%d (tick=%dHz)\n", server_port, RC_TICK_HZ);

    srand((unsigned int)time(NULL));
    memset(g_slots, 0, sizeof(g_slots));
    unsigned int start_now = now_ms();
    for (int i = 1; i < RC_MAX_VEHICLES; i++) {
        g_slots[i].active = 1;
        g_slots[i].is_bot = 1;
        spawn_formation(i);
        pick_bot_waypoint(&g_slots[i], start_now);
    }
    printf("%d bots spawned, slot 0 reserved for the first human to connect.\n", RC_MAX_VEHICLES - 1);

    unsigned int last_tick_ms = now_ms();
    const unsigned int tick_ms = 1000 / RC_TICK_HZ;
    unsigned int server_tick = 0;

    for (;;) {
        char buf[512];
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        ssize_t n;
        while ((n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&from, &from_len)) > 0) {
            if ((size_t)n < sizeof(RcHeader)) continue;
            RcHeader hdr;
            memcpy(&hdr, buf, sizeof(RcHeader));
            if (hdr.type == RC_PACKET_CONNECT) {
                if (!g_slots[0].active) {
                    g_slots[0].active = 1;
                    g_slots[0].is_bot = 0;
                    spawn_formation(0);
                    printf("Human claimed slot 0 from %s:%d\n", inet_ntoa(from.sin_addr), ntohs(from.sin_port));
                }
                g_slots[0].addr = from;
                g_slots[0].addr_len = from_len;
                RcWelcomePacket w;
                memset(&w, 0, sizeof(w));
                w.hdr.type = RC_PACKET_WELCOME;
                w.hdr.client_id = 0;
                w.client_id = 0;
                sendto(sock, &w, sizeof(w), 0, (struct sockaddr *)&g_slots[0].addr, g_slots[0].addr_len);
            } else if (hdr.type == RC_PACKET_USERCMD && (size_t)n >= sizeof(RcUserCmdPacket) && g_slots[0].active) {
                RcUserCmdPacket cmd;
                memcpy(&cmd, buf, sizeof(cmd));
                if (cmd.cmd_sequence >= g_slots[0].latest_cmd_seq) {
                    g_slots[0].latest_cmd_seq = cmd.cmd_sequence;
                    g_slots[0].latest_throttle = cmd.throttle;
                    g_slots[0].latest_steer = cmd.steer;
                    g_slots[0].latest_buttons = cmd.buttons;
                    g_slots[0].last_usercmd_ms = now_ms();
                    /* Reply-to address refreshed on every real packet, not just CONNECT -- covers
                       the client rebinding its own local port across a restart. */
                    g_slots[0].addr = from;
                    g_slots[0].addr_len = from_len;
                }
            }
        }

        unsigned int now = now_ms();
        if (now - last_tick_ms >= tick_ms) {
            last_tick_ms = now;
            server_tick++;

            if (g_slots[0].active && now - g_slots[0].last_usercmd_ms > RC_USERCMD_STALE_MS) {
                g_slots[0].latest_throttle = 0.0f;
                g_slots[0].latest_steer = 0.0f;
                g_slots[0].latest_buttons = 0;
            }

            for (int i = 0; i < RC_MAX_VEHICLES; i++) {
                VehicleSlot *s = &g_slots[i];
                if (!s->active) continue;
                float throttle, steer, buttons_handbrake = 0.0f;
                if (s->is_bot) {
                    float dx = s->bot_target_x - s->sim.x, dz = s->bot_target_z - s->sim.z;
                    if (now >= s->bot_next_retarget_ms || sqrtf(dx * dx + dz * dz) < RC_BOT_ARRIVE_DIST) {
                        pick_bot_waypoint(s, now);
                    }
                    racer_bot_drive_toward(&s->sim, s->bot_target_x, s->bot_target_z, &throttle, &steer);
                    (void)buttons_handbrake; /* bots don't handbrake yet -- real future refinement, not faked here */
                    racer_vehicle_tick(&s->sim, throttle, steer, 0, RC_TICK_DT);
                } else {
                    int handbrake = (s->latest_buttons & RC_BTN_HANDBRAKE) != 0;
                    racer_vehicle_tick(&s->sim, s->latest_throttle, s->latest_steer, handbrake, RC_TICK_DT);
                }
                s->sim.y = racer_terrain_height_at(g_heights, s->sim.x, s->sim.z);
            }

            if (g_slots[0].active) {
                RcSnapshotPacket snap;
                memset(&snap, 0, sizeof(snap));
                snap.hdr.type = RC_PACKET_SNAPSHOT;
                snap.hdr.client_id = 0;
                snap.hdr.sequence = server_tick;
                snap.server_tick = server_tick;
                for (int i = 0; i < RC_MAX_VEHICLES; i++) {
                    snap.active[i] = (unsigned char)g_slots[i].active;
                    snap.is_bot[i] = (unsigned char)g_slots[i].is_bot;
                    snap.vehicles[i].x = g_slots[i].sim.x;
                    snap.vehicles[i].y = g_slots[i].sim.y;
                    snap.vehicles[i].z = g_slots[i].sim.z;
                    snap.vehicles[i].yaw = g_slots[i].sim.yaw;
                    snap.vehicles[i].speed = g_slots[i].sim.speed;
                }
                sendto(sock, &snap, sizeof(snap), 0, (struct sockaddr *)&g_slots[0].addr, g_slots[0].addr_len);
            }

            if (server_tick % (RC_TICK_HZ * 2) == 0) {
                printf("tick=%u slot0=(%.2f,%.2f,%.2f) bot1=(%.2f,%.2f,%.2f)\n",
                       server_tick, g_slots[0].sim.x, g_slots[0].sim.y, g_slots[0].sim.z,
                       g_slots[1].sim.x, g_slots[1].sim.y, g_slots[1].sim.z);
            }
        } else {
            usleep(1000);
        }
    }

    close(sock);
    return 0;
}
