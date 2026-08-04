# WEAKNIGHT: BEDROCK RACERS — NORTHSTAR

## Where this came from

The founder's own pasted pitch (`README.md`) describes a "Vertical Slice 0" that bundles together
what would realistically be several separate, serious projects: a real vehicle physics model
(F1-tier grip curves, downforce, slip angle, lock-up), destructible voxel terrain, a full
boids-flocking traffic/wildlife system, emergent trade routes, cascading power grids, self-healing
city rebuilding, evolving faction AI, and community-hosted multiplayer sync — all at once, with no
existing code, no chosen tech stack, and no repo scaffold yet.

Per this monorepo's own standing principle (the same one `shankpit-460/CLAUDE.md` states for that
fork: "write a real NORTHSTAR.md... before cutting code, per the Emily Way's own spec-before-
implementation"), that pasted spec is the *destination*, not the first commit. This document
exists to turn it into a real, phased, buildable plan — starting from the smallest real proof
point, not the full slice.

**The pasted spec's own success test still holds as the long-term bar**: if a tester says "this is
hard but I want to go faster," the F1 handling passed. If they say "this feels floaty" or "like
Mario Kart," it failed. That's a real, useful acceptance test — just not one Phase 0 needs to
clear yet, because Phase 0 doesn't build the F1 handling model at all (see below).

## Real infrastructure already in this monorepo (reuse, don't reinvent)

Before choosing a tech stack, checked what already exists — the same lesson this session already
had to apply once today, on `shankpit-460` (found and reverted a redundant, server-simulated bot
AI system in favor of an existing, proven, real bot pool rather than building a parallel one):

- **`GoblinFoxDragon/apps2/server-go` + `GoblinFoxDragon/server/worldapi`** — a real, tested,
  already-deployed Bedrock-protocol voxel backend. Generates real per-scene chunk terrain (height
  data, block IDs), already serves GoblinFoxDragon's own Town/Meadow scenes over a real HTTP
  heightmap endpoint, and its own block-ID table is already kept in sync with SHANKPIT by hand
  (per GoblinFoxDragon's own CLAUDE.md). This is real, working "Bedrock backend" — the README's
  own first bullet point — already built.
- **`SHANKPIT` / `shankpit-460`** — a real, proven, server-authoritative UDP FPS core (fixed
  60-ish tick server, `PlayerState`/`UserCmd`/snapshot-broadcast wire protocol, a real C client
  with client-side prediction/reconciliation). Racing needs the exact same real-time,
  low-latency, server-authoritative shape a turn-based MUD (GoblinFoxDragon's own `apps2/mud`)
  does not — this is the right architectural sibling to fork from, not GoblinFoxDragon's MUD side.

**Decision**: Bedrock Racers forks the same real pattern `shankpit-460` itself already is — a UDP
server-authoritative core (closer to `shankpit-460`'s own C server/client shape than a from-scratch
engine) — and talks to `GoblinFoxDragon/server/worldapi`'s own real heightmap/chunk endpoint for
terrain instead of building a second, parallel voxel generator. Vehicle physics, racing-specific
netcode, and destructible-terrain block updates are the real new work this repo actually needs to
build; voxel terrain generation and UDP server-authoritative networking are not.

## Phased plan

### Phase 0 — "a car can drive on real voxel terrain" (the only real Phase 0 goal)

The pasted VS0 spec's own `README.md` says the whole point is proving "a voxel Bedrock-backed
world can support high-speed physics gameplay... while being genuinely fun." Phase 0 proves the
first half of that sentence only — real terrain, a real vehicle, nothing else:

- One real vehicle (arcade-tier handling to start, not the full F1 grip-curve/downforce/slip-angle
  model yet — that's Phase 1, see below) driving around one real chunk of `worldapi`-generated
  voxel terrain.
- Real server-authoritative movement (position/velocity resolved server-side, matching
  `shankpit-460`'s own real pattern), not a client-authoritative stub.
- A minimal real C client rendering the terrain + vehicle, reusing `shankpit-460`'s own client
  rendering/input conventions rather than inventing a new one.
- Single vehicle, single player, no destruction, no AI, no economy, no factions.

**Phase 0 is done when**: a real player can drive a real vehicle around a real chunk of voxel
terrain, server-authoritative, at a stable frame rate, with no fake/scripted movement standing in
for real physics. That's the actual smallest version of the pasted spec's own core claim.

### Phase 1 — real F1-tier handling + destructible terrain

Only after Phase 0 is real and playable:

- The real physics model the pasted spec actually cares about: tire grip curves, downforce
  scaling with speed, braking/lock-up risk, slip angle, momentum conservation.
- Destructible trees/terrain via real raycast + block updates against `worldapi`'s own real chunk
  data, synced to any other connected client (no visual desync).
- Second vehicle (utility/offroad, per the pasted spec's own "at least two vehicles" scope).

### Phase 2 — multiplayer-real

- Real multi-client sync (more than one real player in the same chunk at once), matching
  `shankpit-460`'s own already-proven snapshot/interpolation pattern.
- Community-hostable server, no singleplayer-only shortcuts baked into the netcode.

### Phase 3+ — emergent systems (explicitly deferred, not cut)

Everything the pasted spec calls "Emergent Systems" — boids flocking, trade routes, power grid
cascades, self-healing cities, evolving factions — is real, valuable, and explicitly *not* Phase
0/1/2 scope. Each of these is its own real systems-simulation project in its own right (boids
alone needs a real flocking/obstacle-avoidance implementation; power-grid cascades need a real
dependency graph and failure-propagation model). Building any of them before a car can reliably
drive on real terrain would be building on an unproven foundation. Revisit once Phase 0-2 are real
and shipped, not before.

## Non-goals for now

- No build-macro system (ramps/walls/towers) until Phase 1's destruction system is real — instant
  building without real destruction to counterbalance it isn't the pasted spec's own intent.
- No faction/economy AI before Phase 3.
- No fake animations standing in for real physics at any phase — the pasted spec's own explicit
  non-negotiable ("no fake animations hiding lack of physics... no scripted emergence... no
  hard-coded paths") applies starting Phase 0, not just at the end.

## Open questions (real, not yet decided)

- Exact fork point: a literal `shankpit-460` fork (like `shankpit-460` was itself forked from
  `SHANKPIT`), or a new repo that vendors/imports the relevant shared packages
  (`packages/common/net_sim.h`, `protocol.h`, etc.)? Leaning fork, given how much of
  `shankpit-460`'s own real netcode (ticket auth, snapshot broadcast, client prediction) transfers
  directly — but not decided yet.
- Does `worldapi`'s own real heightmap format need a racing-specific extension (banking, elevation
  changes tuned for a track rather than a walkable town), or is Phase 0 fine using whatever terrain
  it already generates as-is? Phase 0 should answer this empirically, not guess up front.
