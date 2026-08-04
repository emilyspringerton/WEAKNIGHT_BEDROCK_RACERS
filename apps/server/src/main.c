/* WEAKNIGHT_BEDROCK_RACERS -- Phase 0 server (docs/NORTHSTAR.md).
 *
 * "Phase 0 is done when: a real player can drive a real vehicle around a real chunk of voxel
 * terrain, server-authoritative, at a stable frame rate, with no fake/scripted movement standing
 * in for real physics." This is that server: a fixed-tick UDP loop, real position/velocity
 * resolved here (never trusts the client's own claimed position), matching shankpit-460's own
 * server-authoritative pattern -- just for one vehicle's real state instead of a full FPS roster.
 *
 * Terrain is fetched once at startup from GoblinFoxDragon's own real, already-deployed worldapi
 * HTTP endpoint (GET /heightmap on port 7070) -- reused, not reinvented, per NORTHSTAR.md's own
 * "real infrastructure already in this monorepo" decision. The vehicle's Y is set from that real
 * heightmap every tick, not held at a constant -- driving over Meadow's real gentle rolling
 * terrain actually changes the car's height, the whole point of Phase 0's own acceptance test.
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

static unsigned char g_heights[256];
static int g_heights_ready = 0;

/* fetch_heightmap: same real GET /heightmap?scene=&cx=&cz= call GoblinFoxDragon's own
 * battlegrounds_gui client already makes to this exact worldapi service -- one real HTTP round
 * trip at startup, not a fake/local terrain generator. worldapi_host is configurable (defaults to
 * localhost) since Phase 2 will run this server on a different box than GoblinFoxDragon's own. */
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

    printf("WEAKNIGHT_BEDROCK_RACERS server (Phase 0) -- fetching real terrain from worldapi %s:%d...\n",
           worldapi_host, worldapi_port);
    if (!fetch_heightmap(worldapi_host, worldapi_port)) {
        fprintf(stderr, "FATAL: could not load real terrain from worldapi -- refusing to run on fake/flat ground.\n");
        return 1;
    }
    g_heights_ready = 1;
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

    RcVehicleSimState vehicle;
    memset(&vehicle, 0, sizeof(vehicle));
    vehicle.y = racer_terrain_height_at(g_heights, 0.0f, 0.0f);

    int client_connected = 0;
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t client_addr_len = sizeof(client_addr);

    float latest_throttle = 0.0f, latest_steer = 0.0f;
    unsigned int latest_cmd_seq = 0;
    unsigned int last_usercmd_ms = 0;
    unsigned int server_tick = 0;
#define RC_USERCMD_STALE_MS 300 /* real safety net -- a hung/disconnected client must not leave the vehicle stuck at max throttle forever */

    unsigned int last_tick_ms = now_ms();
    const unsigned int tick_ms = 1000 / RC_TICK_HZ;

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
                client_connected = 1;
                client_addr = from;
                client_addr_len = from_len;
                RcWelcomePacket w;
                memset(&w, 0, sizeof(w));
                w.hdr.type = RC_PACKET_WELCOME;
                w.hdr.client_id = 0;
                w.client_id = 0;
                sendto(sock, &w, sizeof(w), 0, (struct sockaddr *)&client_addr, client_addr_len);
                printf("Client connected from %s:%d\n", inet_ntoa(from.sin_addr), ntohs(from.sin_port));
            } else if (hdr.type == RC_PACKET_USERCMD && (size_t)n >= sizeof(RcUserCmdPacket)) {
                RcUserCmdPacket cmd;
                memcpy(&cmd, buf, sizeof(cmd));
                if (cmd.cmd_sequence >= latest_cmd_seq) {
                    latest_cmd_seq = cmd.cmd_sequence;
                    latest_throttle = cmd.throttle;
                    latest_steer = cmd.steer;
                    last_usercmd_ms = now_ms();
                    /* Reply-to address is refreshed on every real packet, not just CONNECT --
                       covers the client rebinding its own local port across a restart without
                       requiring a fresh CONNECT handshake first. */
                    client_addr = from;
                    client_addr_len = from_len;
                    client_connected = 1;
                }
            }
        }

        unsigned int now = now_ms();
        if (now - last_tick_ms >= tick_ms) {
            last_tick_ms = now;
            server_tick++;

            if (client_connected && now - last_usercmd_ms > RC_USERCMD_STALE_MS) {
                latest_throttle = 0.0f;
                latest_steer = 0.0f;
            }
            racer_vehicle_tick(&vehicle, latest_throttle, latest_steer, RC_TICK_DT);
            vehicle.y = racer_terrain_height_at(g_heights, vehicle.x, vehicle.z);

            if (client_connected) {
                RcSnapshotPacket snap;
                memset(&snap, 0, sizeof(snap));
                snap.hdr.type = RC_PACKET_SNAPSHOT;
                snap.hdr.client_id = 0;
                snap.hdr.sequence = server_tick;
                snap.server_tick = server_tick;
                snap.vehicle.x = vehicle.x;
                snap.vehicle.y = vehicle.y;
                snap.vehicle.z = vehicle.z;
                snap.vehicle.yaw = vehicle.yaw;
                snap.vehicle.speed = vehicle.speed;
                sendto(sock, &snap, sizeof(snap), 0, (struct sockaddr *)&client_addr, client_addr_len);
            }

            if (server_tick % (RC_TICK_HZ * 2) == 0) {
                printf("tick=%u pos=(%.2f,%.2f,%.2f) yaw=%.2f speed=%.2f\n",
                       server_tick, vehicle.x, vehicle.y, vehicle.z, vehicle.yaw, vehicle.speed);
            }
        } else {
            usleep(1000);
        }
    }

    close(sock);
    return 0;
}
