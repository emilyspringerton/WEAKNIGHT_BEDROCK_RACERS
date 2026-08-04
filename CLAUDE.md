# WEAKNIGHT_BEDROCK_RACERS

## Mission

A voxel Bedrock-backed racing/systems sandbox — real F1-tier vehicle physics, destructible
terrain, and (later) emergent world simulation (boids traffic, trade routes, power grid cascades,
self-healing cities, evolving factions), built on this monorepo's own existing, proven
infrastructure rather than a from-scratch engine.

**Real, phased plan — see `docs/NORTHSTAR.md` before writing any gameplay code.** The founder's
own original pitch (preserved in `README.md`) describes a full "Vertical Slice 0" that bundles
many separate real milestones together; `docs/NORTHSTAR.md` breaks it into an actual buildable
sequence starting from Phase 0 ("a car can drive on real voxel terrain," nothing else) rather than
attempting the whole slice at once.

## Status

Just created (2026-08-04) — repo scaffold + NORTHSTAR only, no gameplay code yet. Phase 0 (see
NORTHSTAR) is the next real work: a minimal C client/server pair, forked from `shankpit-460`'s own
proven UDP server-authoritative pattern, driving a single vehicle around real voxel terrain served
by `GoblinFoxDragon/server/worldapi`.

## Reused, not reinvented

- **Voxel terrain**: `GoblinFoxDragon/apps2/server-go` + `GoblinFoxDragon/server/worldapi` — real,
  already-deployed Bedrock-protocol chunk/heightmap generation. Do not build a second, parallel
  voxel engine; extend or call into this one.
- **Server-authoritative netcode**: `shankpit-460`'s own real UDP core (`packages/common/
  protocol.h`, `net_sim.h`, snapshot broadcast, client-side prediction/reconciliation, HMAC
  connect-ticket auth). Racing needs the same real-time, low-latency shape SHANKPIT already has —
  fork it, don't reinvent it.

## Related Repos

- `GoblinFoxDragon` — source of the real voxel/Bedrock backend (`apps2/server-go`,
  `server/worldapi`) this repo calls into for terrain.
- `SHANKPIT` / `shankpit-460` — source of the real server-authoritative UDP netcode pattern this
  repo forks for vehicle movement sync.
- `EMILY` — RSI loop / backlog coordination for cross-repo work.

## Commit Protocol (standing instruction)

Always commit and push completed work immediately — don't wait to be asked. This is the default
for every repo in this monorepo.
