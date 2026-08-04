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
