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

**Phase 0 shipped (2026-08-04).** A real UDP server (`apps/server`, fixed 60Hz tick) and a real
SDL2/GL client (`apps/client`) exist and are live-verified end-to-end: the server fetches
`GoblinFoxDragon/server/worldapi`'s real `/heightmap` endpoint (port 7070, already deployed --
reused exactly as this doc's own NORTHSTAR proposed, no second terrain generator built), grounds
a single vehicle's Y to that real terrain every tick, and resolves a real arcade-tier vehicle sim
(`packages/common/racer_vehicle.h`: accel/friction/braking, speed-scaled turn rate) purely
server-side off the client's own UDP `UserCmd` packets -- the client never claims its own
position, matching NORTHSTAR's own "not a client-authoritative stub" bar. The client independently
fetches the same real heightmap, renders it as an actual sloped triangle mesh (not a flat
placeholder), and renders the vehicle wherever the server's snapshot says it really is, via a real
chase camera. Live-verified under Xvfb: real acceleration, real turning, and real terrain-relative
Y all confirmed both via server tick logs and a screenshot showing the vehicle sitting on real
rolling terrain, not floating or clipped. See `EMILY/BACKLOG.md` (2026-08-04, WEAKNIGHT_BEDROCK_
RACERS Phase 0 entry) for the full verification trail.

Not yet done: real WASD-in-a-real-window input has only been exercised via a temp test hook (env
var forcing throttle/steer, reverted before commit) -- `SDL_GetKeyboardState` itself is real,
proven code (the same call GoblinFoxDragon's own Town client already relies on), but a live human
at a real keyboard hasn't driven it yet. Phase 1 (real F1-tier physics, destructible terrain,
second vehicle) is the next real milestone -- see `docs/NORTHSTAR.md`.

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

## Founder Real-Time Direction

Whenever the founder gives real-time direction — a new ask, a correction, a "can we also..." —
route it through `emily observe -s info "Founder real-time: <summary>"` first, even if it isn't
this repo's usual domain, then sprint-plan it into `EMILY/BACKLOG.md` (`emily backlog curate`,
scoped into a real SECTION/sub-item, not just a one-line log), and only then implement. See
`EMILY/docs/THE_EMILY_WAY.md` Principle 18 ("Pave the Cow Paths").

## Frame-Break Reframing

Founder-sourced prompting technique (REDGARDEN/NORTHSTAR.md §28, full origin in
REDGARDEN/docs2/MULTI_AGENT_RD_RESEARCH_NOTES.md §5): given a request, name the underlying
structural/systemic pattern it's one instance of — one level of abstraction up — as an added
lens during planning/triage/judgment calls. Use it to spot the general case behind a specific
ask. It augments judgment, it does not replace doing the work: direct, concrete execution of
the literal task asked for still happens every time.

## Commit Protocol (standing instruction)

Always commit and push completed work immediately — don't wait to be asked. This is the default
for every repo in this monorepo.

Every commit — human-written or produced by automated code paths (git-commit helpers in emily-agent, emily.cli, IDUNA handlers, etc.) — must carry the active `emily session` fingerprint as a `session: <tag>` trailer (blank line, then the trailer). This was silently missing from several independently-implemented automated commit helpers across the monorepo until an audit on 2026-08-10 (founder, real-time: "where in the fuck is my llm session id anywhere"). If you add a new automated git-commit code path anywhere, wire in the session tag the same way — don't assume an existing helper already does it.
