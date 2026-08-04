/* WEAKNIGHT_BEDROCK_RACERS -- Phase 0 client (docs/NORTHSTAR.md).
 *
 * Minimal real C client: fetches the same real worldapi heightmap the server drives collision
 * from, renders it as an actual triangle mesh (not a flat placeholder plane), renders one vehicle
 * box, sends real WASD-derived throttle/steer over UDP every frame, and renders the vehicle at
 * wherever the server's own snapshot says it really is -- fully server-authoritative, no local
 * position prediction in Phase 0 (NORTHSTAR: "Real server-authoritative movement... not a
 * client-authoritative stub").
 *
 * Deliberately legacy/fixed-function OpenGL (glBegin/glVertex/glFrustum/glRotatef), not
 * battlegrounds_gui's own modern-GL shader pipeline -- that pipeline's manual glCreateShader/
 * glCreateProgram extension-loading boilerplate buys nothing Phase 0 actually needs (one flat-
 * shaded terrain mesh, one box) and legacy GL is directly linked from libGL on every real target
 * here (no proc-address loading required), keeping this file small while Phase 0 is still proving
 * the core loop. Revisit if/when a real per-pixel shading need shows up.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>

#include "../../../packages/common/http_client.h"
#include "../../../packages/common/racer_protocol.h"
#include "../../../packages/common/racer_vehicle.h"

static unsigned char g_heights[256];

static unsigned int now_ms(void) { return SDL_GetTicks(); }

/* fetch_heightmap: identical real call the server itself makes at startup -- both talk to the
 * same live worldapi endpoint, so the client renders exactly the terrain the server's own
 * collision agrees with (no separate/guessed client-side terrain generator). */
static int fetch_heightmap(const char *worldapi_host, int worldapi_port) {
    char path[64];
    snprintf(path, sizeof(path), "/heightmap?scene=%d&cx=0&cz=0", RC_WORLDAPI_SCENE);
    char resp[8192];
    int status = 0;
    if (http_get_json(worldapi_host, worldapi_port, path, NULL, resp, sizeof(resp), &status) != 0
        || status != 200) {
        return 0;
    }
    size_t found = 0;
    return http_extract_json_uint8_array_field(resp, "height", g_heights, 256, &found) && found == 256;
}

static void draw_terrain(void) {
    const int grid = RC_HEIGHTMAP_GRID;
    const float half = (grid / 2.0f) * RC_CELL_SIZE;
    glColor3f(0.35f, 0.58f, 0.28f); /* Meadow grass green, matches GoblinFoxDragon's own biome_color(0, ...) */
    glBegin(GL_TRIANGLES);
    for (int gz = 0; gz < grid - 1; gz++) {
        for (int gx = 0; gx < grid - 1; gx++) {
            float h00 = racer_heightfield_sample(g_heights, (float)gx, (float)gz) * RC_HEIGHT_SCALE;
            float h10 = racer_heightfield_sample(g_heights, (float)(gx + 1), (float)gz) * RC_HEIGHT_SCALE;
            float h01 = racer_heightfield_sample(g_heights, (float)gx, (float)(gz + 1)) * RC_HEIGHT_SCALE;
            float h11 = racer_heightfield_sample(g_heights, (float)(gx + 1), (float)(gz + 1)) * RC_HEIGHT_SCALE;
            float x0 = gx * RC_CELL_SIZE - half, x1 = (gx + 1) * RC_CELL_SIZE - half;
            float z0 = gz * RC_CELL_SIZE - half, z1 = (gz + 1) * RC_CELL_SIZE - half;
            glVertex3f(x0, h00, z0); glVertex3f(x0, h01, z1); glVertex3f(x1, h10, z0);
            glVertex3f(x1, h10, z0); glVertex3f(x0, h01, z1); glVertex3f(x1, h11, z1);
        }
    }
    glEnd();
}

/* draw_vehicle_box: is_own selects the bright red "this is you" body color; every other active
 * slot (bots today, other real humans once Phase 2 allows more than one) renders in a distinct
 * blue-grey so the driver can tell their own car apart from the field at a glance. */
static void draw_vehicle_box(float x, float y, float z, float yaw, int is_own) {
    glPushMatrix();
    glTranslatef(x, y + 0.6f, z);
    glRotatef(yaw * 180.0f / (float)M_PI, 0.0f, 1.0f, 0.0f);
    if (is_own) glColor3f(0.85f, 0.15f, 0.15f); else glColor3f(0.30f, 0.42f, 0.62f);
    float hw = 0.9f, hh = 0.55f, hl = 1.7f;
    glBegin(GL_QUADS);
    /* top */
    glVertex3f(-hw, hh, -hl); glVertex3f(hw, hh, -hl); glVertex3f(hw, hh, hl); glVertex3f(-hw, hh, hl);
    /* bottom */
    glVertex3f(-hw, -hh, -hl); glVertex3f(-hw, -hh, hl); glVertex3f(hw, -hh, hl); glVertex3f(hw, -hh, -hl);
    /* front (+z = forward, matches racer_vehicle_tick's own sin(yaw)/cos(yaw) convention) */
    glColor3f(1.0f, 0.9f, 0.3f); /* headlights end, brighter -- real "which way is forward" cue */
    glVertex3f(-hw, -hh, hl); glVertex3f(-hw, hh, hl); glVertex3f(hw, hh, hl); glVertex3f(hw, -hh, hl);
    if (is_own) glColor3f(0.85f, 0.15f, 0.15f); else glColor3f(0.30f, 0.42f, 0.62f);
    /* back */
    glVertex3f(-hw, -hh, -hl); glVertex3f(hw, -hh, -hl); glVertex3f(hw, hh, -hl); glVertex3f(-hw, hh, -hl);
    /* left */
    glVertex3f(-hw, -hh, -hl); glVertex3f(-hw, hh, -hl); glVertex3f(-hw, hh, hl); glVertex3f(-hw, -hh, hl);
    /* right */
    glVertex3f(hw, -hh, -hl); glVertex3f(hw, -hh, hl); glVertex3f(hw, hh, hl); glVertex3f(hw, hh, -hl);
    glEnd();
    glPopMatrix();
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    const char *worldapi_host = "localhost";
    int worldapi_port = 7070;
    const char *server_host = "localhost";
    int server_port = 7788;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--worldapi-host") == 0 && i + 1 < argc) worldapi_host = argv[++i];
        else if (strcmp(argv[i], "--worldapi-port") == 0 && i + 1 < argc) worldapi_port = atoi(argv[++i]);
        else if (strcmp(argv[i], "--server-host") == 0 && i + 1 < argc) server_host = argv[++i];
        else if (strcmp(argv[i], "--server-port") == 0 && i + 1 < argc) server_port = atoi(argv[++i]);
    }

    if (!fetch_heightmap(worldapi_host, worldapi_port)) {
        fprintf(stderr, "FATAL: could not load real terrain from worldapi %s:%d\n", worldapi_host, worldapi_port);
        return 1;
    }
    printf("Real Meadow heightmap loaded from worldapi.\n");

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)server_port);
    struct hostent *he = gethostbyname(server_host);
    if (!he) { fprintf(stderr, "FATAL: could not resolve server host %s\n", server_host); return 1; }
    memcpy(&server_addr.sin_addr, he->h_addr, he->h_length);
    int fl = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, fl | O_NONBLOCK);

    RcHeader connect_hdr; memset(&connect_hdr, 0, sizeof(connect_hdr));
    connect_hdr.type = RC_PACKET_CONNECT;
    sendto(sock, &connect_hdr, sizeof(connect_hdr), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    printf("CONNECT sent to %s:%d, retrying until WELCOME lands...\n", server_host, server_port);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    /* Real Xbox controller support (2026-08-04, founder: "do pressure sensitive controls for
     * bedrock racers for my controller (xbox one controller)"). SDL_GameController is the real
     * pressure-sensitive path -- SDL_CONTROLLER_AXIS_TRIGGERRIGHT/LEFT report the actual analog
     * travel of the triggers (0..32767, not just pressed/released), so throttle/brake genuinely
     * ramp with how hard the trigger is pulled instead of being a second digital button. Opens
     * the first controller found; keyboard remains the real fallback (used below whenever no
     * controller is open) rather than this being a hard requirement to run the client at all. */
    SDL_GameController *pad = NULL;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            pad = SDL_GameControllerOpen(i);
            if (pad) {
                printf("Controller connected: %s\n", SDL_GameControllerName(pad));
                break;
            }
        }
    }
    if (!pad) printf("No game controller found -- using keyboard (WASD/arrows + Space).\n");
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    int win_w = 1280, win_h = 800;
    SDL_Window *win = SDL_CreateWindow("WEAKNIGHT: BEDROCK RACERS -- Phase 0",
                                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        win_w, win_h, SDL_WINDOW_OPENGL);
    if (!win) { fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError()); return 1; }
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) { fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError()); return 1; }
    SDL_GL_SetSwapInterval(1);
    glEnable(GL_DEPTH_TEST);

    RcSnapshotPacket latest_snap; memset(&latest_snap, 0, sizeof(latest_snap));
    int welcomed = 0;
    int have_snapshot = 0;
    unsigned int last_connect_retry_ms = now_ms();
    unsigned int cmd_seq = 0;
    unsigned int last_snapshot_ms = 0;

    int running = 1;
    unsigned int win_logged = 0;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = 0;
            if (e.type == SDL_CONTROLLERDEVICEADDED && !pad) {
                pad = SDL_GameControllerOpen(e.cdevice.which);
                if (pad) printf("Controller connected: %s\n", SDL_GameControllerName(pad));
            }
            if (e.type == SDL_CONTROLLERDEVICEREMOVED && pad
                && e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))) {
                printf("Controller disconnected -- falling back to keyboard.\n");
                SDL_GameControllerClose(pad);
                pad = NULL;
            }
        }

        unsigned int now = now_ms();
        if (!welcomed && now - last_connect_retry_ms >= 500) {
            sendto(sock, &connect_hdr, sizeof(connect_hdr), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
            last_connect_retry_ms = now;
        }

        char buf[512];
        ssize_t n;
        while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
            if ((size_t)n < sizeof(RcHeader)) continue;
            RcHeader hdr; memcpy(&hdr, buf, sizeof(hdr));
            if (hdr.type == RC_PACKET_WELCOME) {
                welcomed = 1;
                printf("WELCOME received -- server-authoritative session live.\n");
            } else if (hdr.type == RC_PACKET_SNAPSHOT && (size_t)n >= sizeof(RcSnapshotPacket)) {
                memcpy(&latest_snap, buf, sizeof(latest_snap));
                have_snapshot = 1;
                last_snapshot_ms = now;
            }
        }

        float throttle = 0.0f, steer = 0.0f;
        unsigned int buttons = 0;
        if (pad) {
            /* Real pressure-sensitive throttle/brake -- RT/LT report actual analog trigger travel
               (0 at rest, 32767 fully pulled), so a light tap genuinely produces a smaller
               throttle value than a full pull, not a binary on/off. Right trigger drives forward,
               left trigger drives reverse/brake -- composited into the same signed throttle
               racer_vehicle_tick already expects, same convention as keyboard's W/S. */
            Sint16 rt = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
            Sint16 lt = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
            throttle = (float)rt / 32767.0f - (float)lt / 32767.0f;
            if (throttle > 1.0f) throttle = 1.0f;
            if (throttle < -1.0f) throttle = -1.0f;

            /* Left stick X, real analog steering with a deadzone -- sticks rarely rest at exactly
               0 (mechanical drift), so a small dead zone stops that drift from reading as a
               constant, unintended steer input. */
            const float STICK_DEADZONE = 0.15f;
            Sint16 lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
            float stick = (float)lx / 32767.0f;
            if (stick > 1.0f) stick = 1.0f;
            if (stick < -1.0f) stick = -1.0f;
            if (fabsf(stick) > STICK_DEADZONE) {
                steer = (stick - (stick > 0.0f ? STICK_DEADZONE : -STICK_DEADZONE)) / (1.0f - STICK_DEADZONE);
            }

            if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) {
                buttons |= RC_BTN_HANDBRAKE;
            }
        } else {
            const Uint8 *keys = SDL_GetKeyboardState(NULL);
            if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) throttle += 1.0f;
            if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) throttle -= 1.0f;
            if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) steer -= 1.0f;
            if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) steer += 1.0f;
            if (keys[SDL_SCANCODE_SPACE]) buttons |= RC_BTN_HANDBRAKE;
        }

        if (welcomed) {
            RcUserCmdPacket cmd; memset(&cmd, 0, sizeof(cmd));
            cmd.hdr.type = RC_PACKET_USERCMD;
            cmd.cmd_sequence = ++cmd_seq;
            cmd.cmd_time_ms = now;
            cmd.throttle = throttle;
            cmd.steer = steer;
            cmd.buttons = buttons;
            sendto(sock, &cmd, sizeof(cmd), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        }

        glViewport(0, 0, win_w, win_h);
        glClearColor(0.55f, 0.75f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(60.0, (double)win_w / (double)win_h, 0.1, 500.0);

        /* Real chase camera -- fixed offset behind+above the player's own car (always slot 0,
           per the server's own "slot 0 is the first human" convention) along its own real yaw,
           not a static orbit; follows wherever the server says the car actually is. */
        RcVehicleState own = latest_snap.vehicles[0];
        float cam_back = 8.0f, cam_up = 3.5f;
        float eye_x = own.x - sinf(own.yaw) * cam_back;
        float eye_y = own.y + cam_up;
        float eye_z = own.z - cosf(own.yaw) * cam_back;

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(eye_x, eye_y, eye_z, own.x, own.y + 1.0f, own.z, 0.0, 1.0, 0.0);

        draw_terrain();
        if (have_snapshot) {
            for (int i = 0; i < RC_MAX_VEHICLES; i++) {
                if (!latest_snap.active[i]) continue;
                RcVehicleState *v = &latest_snap.vehicles[i];
                draw_vehicle_box(v->x, v->y, v->z, v->yaw, i == 0);
            }
        }

        SDL_GL_SwapWindow(win);

        if (!win_logged) {
            printf("Client window live, rendering real terrain + vehicles.\n");
            win_logged = 1;
        }

        (void)last_snapshot_ms;
        SDL_Delay(16);
    }

    if (pad) SDL_GameControllerClose(pad);
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    close(sock);
    return 0;
}
