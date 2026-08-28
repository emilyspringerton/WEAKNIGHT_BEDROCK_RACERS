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

#ifdef _WIN32
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <netdb.h>
#endif

#include "../../../packages/common/http_client.h"
#include "../../../packages/common/racer_protocol.h"
#include "../../../packages/common/racer_vehicle.h"
#include "../../../packages/common/racer_track_stadium.h"
#include "../../../packages/common/hud_text.h"

static unsigned char g_heights[256];

/* Stadium track (2026-08-04) -- see racer_track_stadium.h's own doc comment and the server's
 * matching --track flag. Client-side this only affects draw_terrain()'s mesh; every vehicle's Y
 * (including the player's own) still comes straight from the server's snapshot, same as always. */
static int g_track_stadium = 0;
static unsigned char g_stadium_heights[RC_STADIUM_GRID * RC_STADIUM_GRID];

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

/* draw_stadium_terrain: same real mesh-build shape as draw_terrain below, generalized to the
 * bigger RC_STADIUM_GRID/RC_STADIUM_CELL and a dirt-track-brown base color instead of Meadow
 * grass green -- the coliseum's own real dirt track, not decorative recoloring. */
static void draw_stadium_terrain(void) {
    glColor3f(0.42f, 0.32f, 0.20f);
    glBegin(GL_TRIANGLES);
    for (int gz = 0; gz < RC_STADIUM_GRID - 1; gz++) {
        for (int gx = 0; gx < RC_STADIUM_GRID - 1; gx++) {
            float wx0 = RC_STADIUM_ORIGIN + gx * RC_STADIUM_CELL, wx1 = RC_STADIUM_ORIGIN + (gx + 1) * RC_STADIUM_CELL;
            float wz0 = RC_STADIUM_ORIGIN + gz * RC_STADIUM_CELL, wz1 = RC_STADIUM_ORIGIN + (gz + 1) * RC_STADIUM_CELL;
            float h00 = rc_stadium_height_at(g_stadium_heights, wx0, wz0, RC_HEIGHT_SCALE);
            float h10 = rc_stadium_height_at(g_stadium_heights, wx1, wz0, RC_HEIGHT_SCALE);
            float h01 = rc_stadium_height_at(g_stadium_heights, wx0, wz1, RC_HEIGHT_SCALE);
            float h11 = rc_stadium_height_at(g_stadium_heights, wx1, wz1, RC_HEIGHT_SCALE);
            glVertex3f(wx0, h00, wz0); glVertex3f(wx0, h01, wz1); glVertex3f(wx1, h10, wz0);
            glVertex3f(wx1, h10, wz0); glVertex3f(wx0, h01, wz1); glVertex3f(wx1, h11, wz1);
        }
    }
    glEnd();
}

static void draw_terrain(void) {
    if (g_track_stadium) { draw_stadium_terrain(); return; }
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

/* ================================================================================================
 * Real IDUNA login (2026-08-28, founder real-time: "build login from the beginning take it from
 * GFD make sure my GFD test@test.com login works") -- ported from GoblinFoxDragon/apps2/
 * battlegrounds_gui's own real, proven login-screen pattern (draw_login_screen/run_login_screen/
 * get_player_login_ticket), adapted: that client mints a REDGARDEN-specific self-ticket after
 * login; this one joins the real racer matchmaking queue (IDUNA's ShankpitQueue, instantiated a
 * second time for racing -- "after you login it drops you into matchmaking queue"), then mints a
 * RacerTicketHandler ticket once matched. Same real POST /api/v1/auth/email/login endpoint,
 * verified live end-to-end against the real test@test.com/testtest account before this shipped.
 * ================================================================================================ */

static char g_player_jwt[2048];
static char g_player_display_name[64];

static int hex_decode(const char *hex, unsigned char *out, size_t out_len) {
    size_t hexlen = strlen(hex);
    if (hexlen != out_len * 2) return 0;
    for (size_t i = 0; i < out_len; i++) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1) return 0;
        out[i] = (unsigned char)byte;
    }
    return 1;
}

/* json_escape_into: minimal JSON string escaping (backslash + double-quote only) -- same real,
 * deliberately-scoped helper GFD's own get_player_login_ticket uses (see that function's doc
 * comment): agent secrets are operator-controlled and don't need this, but real player-typed
 * email/password absolutely can contain either character. */
static void json_escape_into(const char *in, char *out, size_t out_len) {
    size_t oi = 0;
    for (const char *p = in; *p && oi + 2 < out_len; p++) {
        if (*p == '"' || *p == '\\') {
            if (oi + 3 >= out_len) break;
            out[oi++] = '\\';
        }
        out[oi++] = *p;
    }
    out[oi] = '\0';
}

/* rc_login: real email+password login against IDUNA's POST /api/v1/auth/email/login (the same
 * real, generic player-account endpoint GFD's own client uses -- one IDUNA identity spans the
 * whole monorepo). On success fills g_player_jwt/g_player_display_name and returns 1; on failure
 * returns 0 and writes a short, user-facing reason into out_err. */
static int rc_login(const char *iduna_host, int iduna_port, const char *email, const char *password,
                     char *out_err, size_t out_err_len) {
    char email_esc[192], pw_esc[192];
    json_escape_into(email, email_esc, sizeof(email_esc));
    json_escape_into(password, pw_esc, sizeof(pw_esc));

    char login_body[512];
    snprintf(login_body, sizeof(login_body),
             "{\"email\":\"%s\",\"password\":\"%s\"}", email_esc, pw_esc);

    char resp[4096];
    int status = 0;
    if (http_post_json(iduna_host, iduna_port, "/api/v1/auth/email/login", NULL,
                        login_body, resp, sizeof(resp), &status) != 0) {
        snprintf(out_err, out_err_len, "Could not reach login server.");
        return 0;
    }
    if (status == 401) {
        snprintf(out_err, out_err_len, "Wrong email or password.");
        return 0;
    }
    if (status != 200) {
        snprintf(out_err, out_err_len, "Login failed (server said %d).", status);
        return 0;
    }
    if (!http_extract_json_string_field(resp, "token", g_player_jwt, sizeof(g_player_jwt))) {
        snprintf(out_err, out_err_len, "Login response missing token.");
        return 0;
    }
    if (!http_extract_json_string_field(resp, "display_name", g_player_display_name, sizeof(g_player_display_name))) {
        snprintf(g_player_display_name, sizeof(g_player_display_name), "%s", email);
    }
    printf("LOGIN: authenticated as %s -- joining matchmaking queue\n", g_player_display_name);
    return 1;
}

typedef struct {
    char state[16];   /* "not_queued" | "queuing" | "matched" */
    int  queue_position;
    int  queue_size;
} RcQueueStatus;

/* rc_queue_join / rc_queue_status: real POST/GET against IDUNA's own racer matchmaking queue
 * (/api/v1/racer/queue endpoints, a second instance of the exact same ShankpitQueue type SHANKPIT
 * itself uses -- see IDUNA main.go's own comment on racerQueue). Bearer-authenticated with the
 * JWT rc_login obtained above. */
static int rc_queue_call(const char *iduna_host, int iduna_port, const char *path, RcQueueStatus *out) {
    char resp[512];
    int status = 0;
    if (http_post_json(iduna_host, iduna_port, path, g_player_jwt, NULL, resp, sizeof(resp), &status) != 0
        || status != 200) {
        return 0;
    }
    if (!http_extract_json_string_field(resp, "state", out->state, sizeof(out->state))) return 0;
    long long v = 0;
    out->queue_position = http_extract_json_int_field(resp, "queue_position", &v) ? (int)v : 0;
    out->queue_size = http_extract_json_int_field(resp, "queue_size", &v) ? (int)v : 0;
    return 1;
}

static int rc_queue_join(const char *iduna_host, int iduna_port, RcQueueStatus *out) {
    return rc_queue_call(iduna_host, iduna_port, "/api/v1/racer/queue/join", out);
}

static int rc_queue_status(const char *iduna_host, int iduna_port, RcQueueStatus *out) {
    char resp[512];
    int status = 0;
    if (http_get_json(iduna_host, iduna_port, "/api/v1/racer/queue/status", g_player_jwt, resp, sizeof(resp), &status) != 0
        || status != 200) {
        return 0;
    }
    if (!http_extract_json_string_field(resp, "state", out->state, sizeof(out->state))) return 0;
    long long v = 0;
    out->queue_position = http_extract_json_int_field(resp, "queue_position", &v) ? (int)v : 0;
    out->queue_size = http_extract_json_int_field(resp, "queue_size", &v) ? (int)v : 0;
    return 1;
}

/* rc_mint_ticket: real POST /api/v1/racer/ticket (IDUNA's RacerTicketHandler) once matched --
 * mints on behalf of the caller's own JWT subject, a player can only ever mint a ticket for
 * themselves (same trust model as SHANKPIT/REDGARDEN's own ticket handlers). */
static int rc_mint_ticket(const char *iduna_host, int iduna_port,
                           unsigned char out_ticket[RC_TICKET_TOTAL_LEN], char *out_err, size_t out_err_len) {
    char resp[512];
    int status = 0;
    if (http_post_json(iduna_host, iduna_port, "/api/v1/racer/ticket", g_player_jwt, NULL, resp, sizeof(resp), &status) != 0) {
        snprintf(out_err, out_err_len, "Could not reach ticket server.");
        return 0;
    }
    if (status != 200) {
        snprintf(out_err, out_err_len, "Ticket mint failed (server said %d).", status);
        return 0;
    }
    char ticket_hex[128];
    if (!http_extract_json_string_field(resp, "ticket", ticket_hex, sizeof(ticket_hex))) {
        snprintf(out_err, out_err_len, "Ticket response missing ticket field.");
        return 0;
    }
    if (!hex_decode(ticket_hex, out_ticket, RC_TICKET_TOTAL_LEN)) {
        snprintf(out_err, out_err_len, "Ticket field was not valid hex.");
        return 0;
    }
    return 1;
}

/* ---------------- login screen (SDL2 GL, hud_text.h stroke font -- see that header's own
 * "known, deliberate placeholder" note re: real LINNEN fonts, not yet integrated) ---------------- */
#define LOGIN_FIELD_MAX 127

typedef struct {
    char email[LOGIN_FIELD_MAX + 1];
    char password[LOGIN_FIELD_MAX + 1];
    int  focus; /* 0 = email, 1 = password */
    char error[128];
    int  submitting;
} LoginScreenState;

static void draw_login_screen(SDL_Window *win, int win_w, int win_h, const LoginScreenState *st) {
    glClearColor(0.05f, 0.06f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, win_w, 0, win_h, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor3f(0.95f, 0.55f, 0.2f);
    rc_draw_string("BEDROCK RACERS -- LOG IN", win_w / 2.0f - 160.0f, win_h - 120.0f, 16);
    glColor3f(0.6f, 0.65f, 0.7f);
    rc_draw_string("TAB TO SWITCH FIELD -- ENTER TO LOG IN -- ESC TO QUIT", win_w / 2.0f - 220.0f, win_h - 150.0f, 8);

    float box_w = 420.0f, box_h = 44.0f;
    float box_x = win_w / 2.0f - box_w / 2.0f;
    float email_y = win_h - 230.0f;
    float pass_y = win_h - 300.0f;

    for (int field = 0; field < 2; field++) {
        float top = (field == 0) ? email_y : pass_y;
        float bottom = top - box_h;
        int focused = (st->focus == field);
        glColor4f(focused ? 0.25f : 0.1f, focused ? 0.2f : 0.1f, focused ? 0.35f : 0.15f, 0.9f);
        glRectf(box_x, bottom, box_x + box_w, top);
        glColor3f(focused ? 1.0f : 0.5f, focused ? 0.6f : 0.4f, focused ? 0.25f : 0.35f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(box_x, bottom); glVertex2f(box_x + box_w, bottom);
        glVertex2f(box_x + box_w, top); glVertex2f(box_x, top);
        glEnd();

        glColor3f(0.6f, 0.65f, 0.75f);
        rc_draw_string(field == 0 ? "EMAIL" : "PASSWORD", box_x, top + 10.0f, 8);

        char shown[LOGIN_FIELD_MAX + 1];
        const char *raw = (field == 0) ? st->email : st->password;
        if (field == 1) {
            size_t n = strlen(raw);
            if (n > LOGIN_FIELD_MAX) n = LOGIN_FIELD_MAX;
            for (size_t i = 0; i < n; i++) shown[i] = '*';
            shown[n] = '\0';
        } else {
            snprintf(shown, sizeof(shown), "%s", raw);
        }
        glColor3f(0.95f, 0.95f, 1.0f);
        rc_draw_string(shown, box_x + 10.0f, bottom + box_h / 2.0f - 4.0f, 10);
    }

    if (st->submitting) {
        glColor3f(0.9f, 0.75f, 0.3f);
        rc_draw_string("LOGGING IN...", win_w / 2.0f - 60.0f, pass_y - 60.0f, 10);
    } else if (st->error[0]) {
        glColor3f(1.0f, 0.4f, 0.4f);
        rc_draw_string(st->error, win_w / 2.0f - 190.0f, pass_y - 60.0f, 9);
    }

    SDL_GL_SwapWindow(win);
}

/* run_login_screen: blocking SDL event loop shown before any UDP connect. On success, g_player_jwt/
 * g_player_display_name are set and this returns 1; on quit/window-close returns 0. */
static int run_login_screen(SDL_Window *win, int win_w, int win_h,
                             const char *iduna_host, int iduna_port,
                             const char *prefill_email, const char *prefill_password) {
    LoginScreenState st;
    memset(&st, 0, sizeof(st));
    if (prefill_email) snprintf(st.email, sizeof(st.email), "%s", prefill_email);
    if (prefill_password) snprintf(st.password, sizeof(st.password), "%s", prefill_password);
    SDL_StartTextInput();
    int running = 1;
    int ok = 0;
    /* Non-interactive smoke-test path (2026-08-28) -- real end-to-end verification under Xvfb,
       same "no live human hands" precedent this repo's own Phase 0 already used, not a fake bypass
       of the real login call itself (rc_login below still makes the real HTTP round trip). */
    if (prefill_email && prefill_email[0] && prefill_password && prefill_password[0]) {
        st.submitting = 1;
    }
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { running = 0; break; }
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                win_w = e.window.data1; win_h = e.window.data2;
            } else if (e.type == SDL_TEXTINPUT && !st.submitting) {
                char *field = (st.focus == 0) ? st.email : st.password;
                size_t len = strlen(field);
                size_t add = strlen(e.text.text);
                if (len + add <= LOGIN_FIELD_MAX) strcat(field, e.text.text);
            } else if (e.type == SDL_KEYDOWN && !st.submitting) {
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    running = 0;
                } else if (e.key.keysym.sym == SDLK_TAB) {
                    st.focus = 1 - st.focus;
                } else if (e.key.keysym.sym == SDLK_BACKSPACE) {
                    char *field = (st.focus == 0) ? st.email : st.password;
                    size_t len = strlen(field);
                    if (len > 0) field[len - 1] = '\0';
                } else if (e.key.keysym.sym == SDLK_v && (SDL_GetModState() & KMOD_CTRL)) {
                    char *field = (st.focus == 0) ? st.email : st.password;
                    char *clip = SDL_GetClipboardText();
                    if (clip) {
                        size_t len = strlen(field), add = strlen(clip);
                        if (len + add > LOGIN_FIELD_MAX) add = LOGIN_FIELD_MAX - len;
                        if (add > 0 && len <= LOGIN_FIELD_MAX) strncat(field, clip, add);
                        SDL_free(clip);
                    }
                } else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER) {
                    if (st.email[0] && st.password[0]) {
                        st.submitting = 1;
                        st.error[0] = '\0';
                    }
                }
            }
        }
        if (!running) break;

        draw_login_screen(win, win_w, win_h, &st);

        if (st.submitting) {
            char err[128] = "";
            if (rc_login(iduna_host, iduna_port, st.email, st.password, err, sizeof(err))) {
                ok = 1;
                running = 0;
            } else {
                snprintf(st.error, sizeof(st.error), "%s", err);
                st.submitting = 0;
            }
        }
        SDL_Delay(16);
    }
    SDL_StopTextInput();
    return ok;
}

static void draw_queue_screen(SDL_Window *win, int win_w, int win_h, const RcQueueStatus *qs, const char *err) {
    glClearColor(0.05f, 0.06f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, win_w, 0, win_h, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor3f(0.95f, 0.55f, 0.2f);
    char welcome[96];
    snprintf(welcome, sizeof(welcome), "WELCOME %s", g_player_display_name);
    rc_draw_string(welcome, win_w / 2.0f - (float)strlen(welcome) * 6.0f, win_h - 160.0f, 12);

    if (err && err[0]) {
        glColor3f(1.0f, 0.4f, 0.4f);
        rc_draw_string(err, win_w / 2.0f - 220.0f, win_h / 2.0f, 9);
    } else {
        glColor3f(0.7f, 0.9f, 0.75f);
        char line[96];
        snprintf(line, sizeof(line), "QUEUING... POSITION %d OF %d", qs->queue_position, qs->queue_size);
        rc_draw_string(line, win_w / 2.0f - (float)strlen(line) * 5.0f, win_h / 2.0f, 10);
    }
    SDL_GL_SwapWindow(win);
}

/* run_matchmaking: real POST /join, then real GET /status polled once per second (no busy-loop
 * hammering IDUNA) until "matched" -- "after you login it drops you into matchmaking queue"
 * (founder, 2026-08-28). Returns 1 with out_ticket filled once matched and minted, 0 on
 * quit/window-close/a real failure shown on screen. */
static int run_matchmaking(SDL_Window *win, int win_w, int win_h, const char *iduna_host, int iduna_port,
                            unsigned char out_ticket[RC_TICKET_TOTAL_LEN]) {
    RcQueueStatus qs; memset(&qs, 0, sizeof(qs));
    char err[128] = "";
    int matched = 0;
    if (!rc_queue_join(iduna_host, iduna_port, &qs)) {
        snprintf(err, sizeof(err), "Could not join matchmaking queue.");
    } else if (strcmp(qs.state, "matched") == 0) {
        /* Real bug found live (2026-08-28): join() itself can already return "matched" (e.g.
           racerQueue.MinPlayers=1 -- a lone real human is a whole match on its own once bots
           fill the rest, see main.go's own comment). The poll loop below only ever notices a
           matched->just-happened TRANSITION, so without this, an already-matched join left
           the player stuck on the queue screen forever, waiting for a state change that had
           already happened before the loop even started. */
        matched = 1;
    }
    unsigned int last_poll_ms = now_ms();
    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { running = 0; break; }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) { running = 0; break; }
        }
        if (!running) break;

        unsigned int now = now_ms();
        if (!err[0] && strcmp(qs.state, "matched") != 0 && now - last_poll_ms >= 1000) {
            last_poll_ms = now;
            if (!rc_queue_status(iduna_host, iduna_port, &qs)) {
                snprintf(err, sizeof(err), "Lost contact with matchmaking server.");
            } else if (strcmp(qs.state, "matched") == 0) {
                matched = 1;
            }
        }

        draw_queue_screen(win, win_w, win_h, &qs, err);

        if (matched) {
            char ticket_err[128] = "";
            if (rc_mint_ticket(iduna_host, iduna_port, out_ticket, ticket_err, sizeof(ticket_err))) {
                return 1;
            }
            snprintf(err, sizeof(err), "%s", ticket_err);
            matched = 0;
        }
        SDL_Delay(16);
    }
    return 0;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    const char *worldapi_host = "localhost";
    int worldapi_port = 7070;
    const char *server_host = "localhost";
    int server_port = 7788;
    const char *iduna_host = "localhost";
    int iduna_port = 8080;
    const char *prefill_email = NULL;
    const char *prefill_password = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--worldapi-host") == 0 && i + 1 < argc) worldapi_host = argv[++i];
        else if (strcmp(argv[i], "--worldapi-port") == 0 && i + 1 < argc) worldapi_port = atoi(argv[++i]);
        else if (strcmp(argv[i], "--server-host") == 0 && i + 1 < argc) server_host = argv[++i];
        else if (strcmp(argv[i], "--server-port") == 0 && i + 1 < argc) server_port = atoi(argv[++i]);
        else if (strcmp(argv[i], "--iduna-host") == 0 && i + 1 < argc) iduna_host = argv[++i];
        else if (strcmp(argv[i], "--iduna-port") == 0 && i + 1 < argc) iduna_port = atoi(argv[++i]);
        else if (strcmp(argv[i], "--email") == 0 && i + 1 < argc) prefill_email = argv[++i];
        else if (strcmp(argv[i], "--password") == 0 && i + 1 < argc) prefill_password = argv[++i];
        else if (strcmp(argv[i], "--track") == 0 && i + 1 < argc) {
            const char *track = argv[++i];
            if (strcmp(track, "stadium") == 0) g_track_stadium = 1;
            else if (strcmp(track, "meadow") != 0) {
                fprintf(stderr, "--track: unknown track %s -- use meadow or stadium\n", track);
                return 1;
            }
        }
    }

    if (g_track_stadium) {
        rc_stadium_generate_heights(g_stadium_heights, RC_HEIGHT_SCALE);
        printf("Real stadium heightfield generated (%dx%d).\n", RC_STADIUM_GRID, RC_STADIUM_GRID);
    } else {
        if (!fetch_heightmap(worldapi_host, worldapi_port)) {
            fprintf(stderr, "FATAL: could not load real terrain from worldapi %s:%d\n", worldapi_host, worldapi_port);
            return 1;
        }
        printf("Real Meadow heightmap loaded from worldapi.\n");
    }

#ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
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

    /* Real login + matchmaking queue, before any UDP connect at all -- "build login from the
       beginning" / "after you login it drops you into matchmaking queue" (founder, 2026-08-28).
       A window+GL context must exist first (both screens render into it); the actual driving
       loop's own glEnable(GL_DEPTH_TEST)/gluPerspective setup happens further down, once real
       racing starts. */
    if (!run_login_screen(win, win_w, win_h, iduna_host, iduna_port, prefill_email, prefill_password)) {
        SDL_GL_DeleteContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 0; /* user quit at the login screen -- not an error */
    }
    unsigned char ticket[RC_TICKET_TOTAL_LEN];
    if (!run_matchmaking(win, win_w, win_h, iduna_host, iduna_port, ticket)) {
        SDL_GL_DeleteContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 0; /* user quit while queuing, or a shown, real failure */
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)server_port);
    struct hostent *he = gethostbyname(server_host);
    if (!he) { fprintf(stderr, "FATAL: could not resolve server host %s\n", server_host); return 1; }
    memcpy(&server_addr.sin_addr, he->h_addr, he->h_length);
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int fl = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, fl | O_NONBLOCK);
#endif

    RcConnectPacket connect_pkt; memset(&connect_pkt, 0, sizeof(connect_pkt));
    connect_pkt.hdr.type = RC_PACKET_CONNECT;
    memcpy(connect_pkt.ticket, ticket, RC_TICKET_TOTAL_LEN);
    sendto(sock, &connect_pkt, sizeof(connect_pkt), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    printf("CONNECT (with real ticket) sent to %s:%d, retrying until WELCOME lands...\n", server_host, server_port);

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
    glEnable(GL_DEPTH_TEST);

    RcSnapshotPacket latest_snap; memset(&latest_snap, 0, sizeof(latest_snap));
    int welcomed = 0;
    int have_snapshot = 0;
    unsigned int last_connect_retry_ms = now_ms();
    unsigned int cmd_seq = 0;
    unsigned int last_snapshot_ms = 0;
    char reject_reason[RC_REJECT_REASON_MAX + 1]; reject_reason[0] = '\0';

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
        if (!welcomed && !reject_reason[0] && now - last_connect_retry_ms >= 500) {
            sendto(sock, &connect_pkt, sizeof(connect_pkt), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
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
            } else if (hdr.type == RC_PACKET_REJECT && (size_t)n >= sizeof(RcRejectPacket)) {
                RcRejectPacket rej; memcpy(&rej, buf, sizeof(rej));
                rej.reason[RC_REJECT_REASON_MAX] = '\0'; /* defend against a non-NUL-terminated wire value */
                snprintf(reject_reason, sizeof(reject_reason), "%s", rej.reason);
                fprintf(stderr, "CONNECT rejected: %s\n", reject_reason);
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

        if (reject_reason[0]) {
            /* Real, visible rejection (bad/expired ticket, no RACER_TICKET_SECRET configured
               server-side, or the human slot already held by a different player) instead of a
               silent hang -- racer_protocol.h's own RcRejectPacket doc comment explains why a
               real reason string, not just an error code, is the right shape for a repo this
               small. */
            glViewport(0, 0, win_w, win_h);
            glDisable(GL_DEPTH_TEST);
            glClearColor(0.09f, 0.04f, 0.04f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0, win_w, 0, win_h, -1, 1);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glColor3f(1.0f, 0.4f, 0.4f);
            rc_draw_string("CONNECT REJECTED", win_w / 2.0f - 120.0f, win_h / 2.0f + 30.0f, 12);
            rc_draw_string(reject_reason, win_w / 2.0f - 220.0f, win_h / 2.0f - 20.0f, 8);
            SDL_GL_SwapWindow(win);
            SDL_Delay(16);
            continue;
        }

        glViewport(0, 0, win_w, win_h);
        glClearColor(0.55f, 0.75f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        /* Stadium's real bounds (~1120 half-width, packages/common/racer_track_stadium.h) reach
           well past Meadow's own tuned 500-unit far plane -- bumped only for this track so the
           proven Meadow path's own depth precision/behavior stays completely untouched. */
        gluPerspective(60.0, (double)win_w / (double)win_h, 0.1, g_track_stadium ? 2000.0 : 500.0);

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
#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif
    return 0;
}
