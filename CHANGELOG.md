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
