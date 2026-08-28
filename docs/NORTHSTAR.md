# WEAKNIGHT: BEDROCK RACERS — NORTHSTAR

## PIVOT (2026-08-28): racer-first, forked from the pre-EINHORN SHANKPIT construct

Founder real-time, verbatim, in order (obs `2026-08-28T16-58-57Z`, Apple #16616):

> "i found an old shankpit experience with a city and 2 diferent cars on fg its like 2 different
> gears in a car almost and the driving is crisp i want to build on top of this experience the
> third person combat wasnt working good but we can figure that out later bedrock racers is a
> racer first" / "we want to use that construct to build out the backend systems the same as we
> have it for the more mature games ... networking wise" / "i want the experience to be like that
> this is like the 460 fork but even earlier and we found like an arena league of legends mode
> already built in" / "we will unify or borrow from redgarden for game systems to make it more
> real if they help i dunno" / "i want the thing to embed parena into it deep into the core of it
> BEDROCKRACERS" / "that is the current pivot i want that game to be like an 8 player network
> game" / "have it do whatever it is doing in the new construct file we will build on top of it"
> / "that was pre einhorn pivot pure fartco bliss" (dating the find: this predates the
> EINHORN_INDUSTRIAL rebrand entirely — an artifact from the old FartCo era of the universe) /
> "you will want to have a separate whole build for the new one" / "start from scratch build
> parena in i will tell you if the vibes are good the map and the phyysics and the game type are
> mostly good some fo the power ups need tweaking"

**Important correction on HOW the construct gets used**: not a literal C-code port. The last
quote above settles it — Bedrock Racers is a **from-scratch build with PARENA embedded from day
one** (not bolted on in a later phase over ported C, as an earlier draft of this plan had it).
`SHANKPIT_CONSTRUCT.txt` is the **reference** for what already feels right — the founder's own
judgment call is "the map and the physics and the game type are mostly good" (the city-scale
map, the buggy/bike gear-feel physics table above, and racing itself as the game type all pass),
with power-ups named as the one piece already known to need tuning later, not a pass/fail verdict
on anything else. Vibes get checked against the founder directly as this gets built, not assumed.

**What was actually found**: `SHANKPIT_CONSTRUCT.txt` — a full single-file source dump this
repo's own CI "source construct step" (commit `874ec6c`) produces from `shankpit-460`, pulled in
by this session's `git pull --rebase`. It captures a real, working, much earlier build of the
SHANKPIT lineage than either `SHANKPIT/` or `shankpit-460/` run today — no HMAC connect-ticket
auth yet (that's a later hardening pass, still real and worth keeping for Phase-2-and-on
multiplayer), but a full **city scene** (`SCENE_CITY`, `packages/rts`-driven NPC districts,
boids, a "Huntsman" city boss) with **two real, tuned vehicle archetypes** already server-
resolved and drivable:

| | `VEH_BUGGY` | `VEH_BIKE` |
|---|---|---|
| Top speed | `BUGGY_MAX_SPEED` 2.5 | `BIKE_MAX_SPEED` 3.8 (5-speed gearbox) |
| Accel | flat | `BIKE_GEAR_ACCEL[1..5]` = 0.14 → 0.07 (falls off per gear, real shifting feel) |
| Friction | `BUGGY_FRICTION` 0.03 | `BIKE_FRICTION` 0.02 |
| Gravity | `BUGGY_GRAVITY` 0.15 | `BIKE_GRAVITY` 0.12 |
| Drift lateral grip | `BUGGY_DRIFT_LATERAL_GRIP` 2.0 | `BIKE_DRIFT_LATERAL_GRIP` 4.0 |

(`SHANKPIT_CONSTRUCT.txt` lines 4696-4718, 6124-6126, 6192-6193, 5651-5717, 9161-9175 — the real
gear-shift logic itself, upshifting/downshifting the bike off its own current speed against
`BIKE_GEAR_MAX`, is what the founder means by "like 2 different gears in a car almost.") This is
the real "crisp driving" the founder wants Bedrock Racers built on top of — a strictly better
starting point than this doc's own original Phase-0 single-vehicle arcade model below, which
never had a gear system or a second vehicle class at all.

The same construct also carries a full **third-person combat layer** (weapons, KO state, the
"Huntsman" boss fight) riding on top of the same city/vehicle core — per the founder, this part
**wasn't working well and is explicitly deferred, not ported**: *"bedrock racers is a racer
first."* It also carries a real **card-based arena/MOBA mode** (`packages/rts/card_system.h` +
`entity_behaviors.h` + `grid_tick.h`: a Clash-Royale-shaped lane-push game — `MAX_HAND_SIZE` 5,
`MAX_DECK_SIZE` 30, influence-cost cards spawning 16 unit types + 8 structure types across 4 tech
levels) — this is the "arena league of legends mode already built in" the founder means; it's
real, already-written code, kept as a later optional mode riding alongside racing, not the
Phase-A focus.

### Revised phased plan (supersedes the original Phase 0-3 plan below for sequencing; Phase 0's
own shipped code is not thrown away — see "What Phase 0 already proved" note further down)

- **Phase A — from-scratch city + two-vehicle driving core, PARENA in from day one.** Not a C-code
  port of `SHANKPIT_CONSTRUCT.txt` — a fresh build, in this repo's own separate, whole build (own
  Makefile/CI, own binaries, per the founder's own explicit call — not shared with
  `shankpit-460`'s build even though the source lineage inspires it), with PARENA already load-
  bearing in the vehicle/gameplay logic rather than added later over existing C. Reference target,
  not a spec to copy verbatim: a `SCENE_CITY`-scale map and two tuned vehicle classes matching the
  real feel of `VEH_BUGGY`/`VEH_BIKE` above (including the bike's real gear-shift curve) — the
  founder's own read on the construct is "the map and the physics and the game type are mostly
  good," so this is the bar to hit, not blind copying. No third-person combat layer — "racer
  first," per the founder.
- **Phase B — grow the netcode toward the "more mature games" shape.** Today's
  `packages/common/racer_protocol.h` is a deliberately minimal Phase-0 wire protocol (see its own
  header comment). Grow it using `shankpit-460`'s own *current*, more hardened server-authoritative
  pattern as the target shape (fixed-tick UDP core, snapshot broadcast, client-side prediction/
  reconciliation, HMAC connect-ticket auth — CLAUDE.md's "Reused, not reinvented" section below
  already names this) — the construct's own `apps/lobby` (mode-select menu: Battle/TDM/CTF/
  Evolution/Join — real precedent for a race-mode picker) and `services/master-server/main.go`
  (a real, if simple, Go TCP matchmaker + broadcast hub — `MatchSize` constant, a queue, a
  2-second matchmaker tick) are the concrete backend-service shape to build the lobby/matchmaking
  layer on, sized up from `MatchSize = 2` toward real races. `RC_MAX_VEHICLES = 8` is already
  correctly sized for this — matches the founder's own **"8 player network game"** target as-is,
  no protocol resize needed, just real matchmaking + snapshot sync at that scale.
- **Phase C — power-ups, tuned against real founder feedback.** The construct's own power-up
  layer is the one piece the founder flagged as already known to need tweaking ("some fo the
  power ups need tweaking") — build a real first pass, then iterate against direct founder
  playtest verdicts ("i will tell you if the vibes are good") rather than trying to guess the
  right tuning up front.
- **Phase D — the arena/card mode as a real optional mode.** Only after Phases A-C are real:
  build a selectable arena/MOBA mode in the same PARENA-first spirit as Phase A, using the
  construct's own already-real `packages/rts/card_system.h` + `entity_behaviors.h` + `grid_tick.h`
  (Clash-Royale-shaped: `MAX_HAND_SIZE` 5, `MAX_DECK_SIZE` 30, influence-cost cards spawning 16
  unit types + 8 structure types across 4 tech levels — real, already-written reference design,
  the "arena league of legends mode already built in" the founder means) as the reference design,
  with its card-selection/AI-opponent decision logic authored in PARENA per `ECOWAR`'s own
  precedent (root `CLAUDE.md`: "first mod to do real PARENA decision logic, not just a trigger") —
  mirroring the construct's own `apps/lobby` mode-select pattern for how a player picks it.
- **Phase E — REDGARDEN cross-pollination (tentative, not committed).** Founder: *"we will unify
  or borrow from redgarden for game systems to make it more real if they help i dunno"* — explicit
  low-confidence framing, unlike Phases A-D. `REDGARDEN`'s own `apps/arena`/`apps/arena_server`
  and `ECOWAR`'s 16-card PARENA-driven system are the concrete candidates if/when this gets
  revisited; nothing here is scoped or committed yet.

**What Phase 0 already proved, still real**: the original Phase 0 (below) shipped a real
server-authoritative single-vehicle sim grounded on `GoblinFoxDragon/server/worldapi`'s real
heightmap terrain, live-verified end-to-end. That terrain-sourcing decision (reuse `worldapi`,
don't build a second voxel generator) stands unchanged by this pivot — Phase A rebuilds the
vehicle/gameplay logic from scratch around PARENA, but has no reason to also reinvent terrain
sourcing; `worldapi` stays the real backend underneath the from-scratch city map.

## Where this came from

*(Original scoping pass, 2026-08-04 — kept for history. The pivot above supersedes its Phase 0-3
sequencing and its "fork `shankpit-460` for netcode" framing (now: build fresh, PARENA-first,
using the older `SHANKPIT_CONSTRUCT.txt` snapshot as reference); the terrain-reuse decision and
the underlying spec-before-code discipline both still hold.)*

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
