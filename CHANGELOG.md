## 2026-08-28
- Real IDUNA login -> matchmaking queue -> HMAC connect-ticket flow, ported from GFD's proven battlegrounds_gui login screen. Verified live end-to-end against test@test.com/testtest. RcConnectPacket now carries a real ticket (fail-closed server verification); RcRejectPacket added for real, visible rejections. (sess-20260825-1938-f6bd411e)

- NORTHSTAR pivot: racer-first, PARENA-embedded-from-day-one, grounded in the pre-EINHORN SHANKPIT_CONSTRUCT.txt reference (two-vehicle gear-shift feel, city map, deferred combat, optional arena/card mode later). See docs/NORTHSTAR.md PIVOT section. (sess-20260825-1938-f6bd411e)

## 2026-08-25

- added auto-release CI job (PITVIPER pattern): real, non-prerelease GitHub release on every push to main (sess-20260825-1938-f6bd411e)

## 2026-08-10

- fix(net): run.bat pointed at localhost for both worldapi and the game server, which only ever works for a same-box test client -- every real downloaded client would FATAL on terrain load exactly like today's live report. Fixed to point at the real box (198.58.107.85). Also: neither racer_server nor the CI-built worldapi dependency (GoblinFoxDragon's gfd-server-go.service) were ever actually deployed persistently -- racer_server only existed as a CI build-verification artifact, never run. Built it, stood it up under a new systemd user unit (ops/systemd/weaknight-racers-server.service, same supervised pattern as shankpit-460's own units, Restart=on-failure), confirmed real terrain + bot spawns + 60Hz tick. Live-verified end-to-end against the real public IP (not loopback): heightmap fetch, UDP connect, WELCOME, real snapshot rendering, all confirmed working over the same host the fixed run.bat now uses. (sess-20260809-1420-e9d3d7f8)

## 2026-08-09

- feat(ci): Windows client cross-compile + SDL2 bundle + run.bat, mirroring SHANKPIT/REDGARDEN's proven mingw pattern — added _WIN32/winsock guards to apps/client/src/main.c so it actually cross-compiles; server stays Linux-only, matching both sibling repos (sess-20260809-1420-e9d3d7f8)

## 2026-08-06

- Added .github/workflows/ci.yml -- builds the real server and SDL2/GL client on push/PR. Compile commands verified locally first (clean gcc build, both binaries). No packaging/Windows cross-compile yet -- not claimed as distributable in docs, added when Phase 1+ actually needs it. (sess-20260723-2347-df115bd5)

## 2026-08-04 (4)
- --track stadium: SHANKPIT's coliseum/dirt-track map ported as a self-contained deterministic heightfield (racer_track_stadium.h), selectable alongside the real worldapi-backed Meadow track (sess-20260723-2347-df115bd5)

- feat(server,client): 8-slot bot match -- 1 human + 7 real autonomous bots. Founder: "can we get
  8 player online bot matches? same pattern as before 7 bots so i can queue into a game." Same
  real shape shankpit-460's own bot pool used: a fixed slot count, client boots straight into a
  running match (direct-connect, no separate matchmaker/lobby service yet -- that fork's own
  "bring the lobby back once bot matches work" framing applies here too). `RC_MAX_VEHICLES=8` in
  `packages/common/racer_protocol.h`; `RcSnapshotPacket` now carries all 8 vehicles' state plus
  active/is_bot flags instead of one. Slot 0 is reserved for the first human to `CONNECT`; slots
  1-7 spawn active and bot-controlled at server start, spread around a real starting circle (not
  stacked at the origin). New `racer_bot_drive_toward` (`packages/common/racer_vehicle.h`): real
  reactive waypoint-seeking through the exact same `racer_vehicle_tick` physics every other
  vehicle uses (no cheating/teleporting, no pre-baked path) -- each bot picks a randomized point
  inside the real terrain chunk every few seconds (or early if it arrives), derives throttle/steer
  fresh every tick from its own current heading error, and slows down for sharp turns instead of
  driving blind. Client renders every active slot, its own car red and everyone else blue-grey,
  camera anchored on slot 0. Live-verified: server log showed real bot movement and heading changes
  before any human ever connected (autonomous, not idle placeholders), and a live screenshot after
  a human joined showed five distinct bot cars at different positions and orientations (independent
  real headings, not a shared scripted path) alongside the player's own car. `gcc -Wall -Wextra`
  clean on both binaries.

## 2026-08-04 (3)

- feat(client): real Xbox controller support -- pressure-sensitive triggers, analog steering.
  Founder: "do pressure sensitive controls for bedrock racers for my controller (xbox one
  controller)." Client now opens the first `SDL_GameController` found at startup (or hot-plugs
  one via `SDL_CONTROLLERDEVICEADDED`/`REMOVED`), falling back to the existing WASD/arrows/Space
  keyboard scheme whenever none is connected -- controller support is additive, not a hard
  requirement to run. Right trigger drives forward throttle and left trigger drives reverse/brake,
  both read via `SDL_CONTROLLER_AXIS_TRIGGERRIGHT`/`LEFT`'s real analog travel (0..32767) and
  composited into the same signed throttle the sim already expects -- a light tap genuinely
  produces less throttle than a full pull, not a second digital button standing in for "pressure
  sensitive." Left stick X drives steering with a real 0.15 dead zone (mechanical stick drift
  never reads as unintended input). Left bumper is the controller's handbrake, same
  `RC_BTN_HANDBRAKE` bit the keyboard's Space already sends. Verified for real, not just compiled:
  built a small standalone harness confirming SDL's virtual-joystick API round-trips real
  axis/button values through `SDL_GameController` in this environment, then temporarily wired an
  `RC_TEST_VIRTUAL_PAD`-gated virtual pad directly into the real client binary (reverted before
  commit) forcing a real half-pulled right trigger and a full-right stick -- the live server log
  showed genuine partial-then-ramping acceleration and a continuous turn at exactly
  `RC_TURN_RATE_MAX` (2.2 rad/s, confirmed against real tick-over-tick yaw deltas), End-to-end,
  analog input in, real physics out. `gcc -Wall -Wextra` clean.

## 2026-08-04 (2)

- feat(vehicle): real handbrake -- locked-wheel deceleration + drift-tier turning. Founder:
  "ensure handbreak is implemented." New `RC_BTN_HANDBRAKE` in `RcUserCmdPacket.buttons`
  (`packages/common/racer_protocol.h`), sent from the client on Space. `racer_vehicle_tick`
  (`packages/common/racer_vehicle.h`) now takes a real `handbrake` flag: when set, it overrides
  throttle entirely and decelerates at `RC_HANDBRAKE_DECEL` (32 u/s^2, vs. `RC_FRICTION_DECEL`'s
  6 u/s^2 passive coast) and swaps the normal speed-scaled turn-rate cap for a flat, higher
  `RC_HANDBRAKE_TURN_RATE_MAX` (3.6 rad/s vs. `RC_TURN_RATE_MAX`'s 2.2 rad/s ceiling) once the car
  has enough real speed to rotate around (`RC_HANDBRAKE_MIN_SPEED_FOR_TURN`) -- a real
  lost-rear-traction handbrake-turn feel, not just a relabeled brake. Server parses the button and
  resets it alongside throttle/steer under the existing stale-usercmd safety net, so a hung client
  can't leave the handbrake stuck on either. Live-verified against the real server with a raw UDP
  test client: passive friction cost 1.3 u/s of speed over ~0.2s (matches the real 6 u/s^2 rate);
  the handbrake cost 6.93 u/s over the same real window (matches 32 u/s^2 almost exactly) and
  produced a real 0.78 rad heading change at full steer in that window -- an effective 3.6 rad/s,
  exactly `RC_HANDBRAKE_TURN_RATE_MAX`, confirmed against live snapshot data, not asserted.
  `gcc -Wall -Wextra` clean on both binaries.

## 2026-08-04

- feat(server, client): Phase 0 -- a real vehicle drives on real voxel terrain, server-authoritative.
  Founder: "go phase 0". New `apps/server` (C, UDP, fixed 60Hz tick): fetches
  `GoblinFoxDragon/server/worldapi`'s real, already-deployed `/heightmap` HTTP endpoint (port 7070)
  at startup, resolves a real arcade-tier vehicle sim (`packages/common/racer_vehicle.h`: real
  accel/friction/braking, speed-scaled turn rate, terrain-relative Y every tick) purely from the
  client's own UDP `UserCmd` packets -- never trusts a client-claimed position. A stale-input
  safety net (found live during verification: a hung/disconnected client left the vehicle stuck at
  max throttle forever) now zeros throttle/steer if no `UserCmd` lands within 300ms. New
  `apps/client` (C, SDL2 + legacy GL): fetches the same real heightmap, renders it as an actual
  sloped triangle mesh, renders the vehicle wherever the server's own snapshot says it really is
  (fully server-authoritative, no client-side prediction yet), real chase camera. `packages/common`
  ported `http_client.h`/`mat4.h` verbatim from GoblinFoxDragon's own proven implementation (real
  reuse, not reinvention, per NORTHSTAR's own "check for existing infra first" decision) and added
  new `racer_protocol.h`/`racer_vehicle.h` sized for one vehicle's real state instead of forking
  shankpit-460's much larger FPS `protocol.h` wholesale. Live-verified end-to-end: a real UDP test
  client drove the server vehicle through real accel/turn/coast-to-stop (confirmed via raw snapshot
  data), and the SDL2 client, run under Xvfb with a temp env-var test hook standing in for real
  keyboard input (reverted before commit), showed the vehicle actually moving across real rolling
  Meadow terrain in a live screenshot -- not a static placeholder. `gcc -Wall -Wextra` clean on
  both binaries.
