# WEAKNIGHT_BEDROCK_RACERS

## Mission

**Racer first** (founder, 2026-08-28 pivot: "bedrock racers is a racer first"). A from-scratch,
PARENA-embedded-from-day-one racing game — city-scale map, two tuned vehicle classes (crisp
gear-shifting handling), real 8-player networked matches — built with `SHANKPIT_CONSTRUCT.txt`
(a real, much-earlier, pre-EINHORN-rebrand SHANKPIT snapshot, pulled into this repo by CI's
"source construct step") as the *reference* for map/physics/game-type feel, not as ported code.
Third-person combat and the arena/MOBA card mode both existed in that older build too; combat is
explicitly deferred/dropped, the card mode is a later optional mode, not the near-term focus.
Emergent-systems scope from the original pitch (boids, trade routes, power grids, factions) is
still real and still deferred, unchanged by this pivot.

**Real, phased plan — see `docs/NORTHSTAR.md` before writing any gameplay code.** The "PIVOT
(2026-08-28)" section at the top of that doc is the current, authoritative direction and
supersedes the original Phase 0-3 sequencing below it (kept for history, not deleted).

## Status

**Phase 0 (single-vehicle arcade prototype) shipped 2026-08-04** — real UDP server + SDL2/GL
client, live-verified server-authoritative movement on real `worldapi` heightmap terrain. See
`docs/NORTHSTAR.md`'s own "What Phase 0 already proved" note: the terrain-sourcing decision this
proved stands unchanged, but the single-vehicle arcade model and Phase 0's own netcode are being
superseded by the 2026-08-28 pivot's Phase A (city + two real vehicle classes, PARENA-first,
built fresh rather than grown from Phase 0's code) — not because Phase 0 failed, but because the
newly-found `SHANKPIT_CONSTRUCT.txt` reference is a strictly better starting feel (real gear
shifting, two vehicle archetypes, a real city map) to build the racer-first direction on top of.

Pivot work itself (Phase A onward) is freshly scoped as of this session — not started yet past
this NORTHSTAR/CLAUDE.md documentation pass. See `EMILY/BACKLOG.md` SECTION (pivot entry, same
date) for the live tracking.

## Reused, not reinvented

- **Voxel terrain**: `GoblinFoxDragon/apps2/server-go` + `GoblinFoxDragon/server/worldapi` — real,
  already-deployed Bedrock-protocol chunk/heightmap generation. Do not build a second, parallel
  voxel engine; extend or call into this one. Unchanged by the 2026-08-28 pivot.
- **Netcode target shape**: `shankpit-460`'s own *current*, more hardened server-authoritative UDP
  core (fixed-tick, snapshot broadcast, client-side prediction/reconciliation, HMAC connect-ticket
  auth) is the shape to grow this repo's own wire protocol toward (`docs/NORTHSTAR.md` Phase B) —
  reference, not a literal fork/shared build; see the pivot's own "separate whole build" call.
- **PARENA** — embedded deep into the gameplay/decision-logic core from day one (`docs/
  NORTHSTAR.md` Phase A/C), not bolted on later. Follows `ECOWAR`'s own precedent as the first mod
  to do real PARENA decision logic rather than just a trigger.
- **`SHANKPIT_CONSTRUCT.txt`** (this repo, root) — the real reference snapshot for map/physics/
  game-type/power-up feel; see `docs/NORTHSTAR.md`'s PIVOT section for the concrete citations
  (line numbers) into it. Reference to build against, not code to port verbatim.

## Related Repos

- `GoblinFoxDragon` — source of the real voxel/Bedrock backend (`apps2/server-go`,
  `server/worldapi`) this repo calls into for terrain.
- `SHANKPIT` / `shankpit-460` — source of `SHANKPIT_CONSTRUCT.txt` (the pre-EINHORN reference
  build this pivot is grounded in) and of the current, more hardened netcode pattern Phase B
  targets.
- `PARENA` — the language embedded deep into this repo's own gameplay/decision-logic core,
  per the 2026-08-28 pivot.
- `ECOWAR` / `REDGARDEN` — `ECOWAR`'s 16-card PARENA-driven decision logic is the direct precedent
  for Phase C/D's own PARENA-in-core approach; `REDGARDEN`'s `apps/arena`/`apps/arena_server` are
  a tentative, not-yet-committed Phase E cross-pollination candidate.
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
