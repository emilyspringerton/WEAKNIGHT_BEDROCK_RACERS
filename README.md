# WEAKNIGHT_BEDROCK_RACERS

## Current Status (2026-08-04)

Real, phased plan lives in [`docs/NORTHSTAR.md`](docs/NORTHSTAR.md) (golden-indexed) — the pitch
below is the long-term destination, not the current build order. **Shipped and live-verified:**
Phase 0 (real vehicle, server-authoritative, on real `worldapi`-generated voxel terrain), a real
handbrake (locked-wheel decel + drift-tier turning), Xbox controller support (pressure-sensitive
triggers, analog steer), and an 8-slot bot match (1 human + 7 real autonomous bots, each
independently pathing via `racer_bot_drive_toward`). Not started: Phase 1 (real F1-tier physics,
destructible terrain), Phase 2 (real multiplayer sync), Phase 3+ (emergent systems). See
`EMILY/BACKLOG.md` for the full real verification trail behind each of these.

---

Love it. Let’s treat this like a real studio vertical slice with **hard acceptance criteria** and a **Definition of Done** that forces the game to be fun, fast, and technically real — not a tech demo.

I’ll structure this like a professional product + gameplay spec.

---

# 🏎️ WEAKNIGHT: BEDROCK RACERS

## Vertical Slice 0 — Acceptance Criteria & Definition of Done

### 🎯 Core Goal of Vertical Slice 0

Prove that:

> A voxel Bedrock-backed world can support **high-speed physics gameplay (F1-tier), destructible environments, emergent systems, and multiplayer-scale infrastructure** — while being genuinely fun.

If this slice works:

👉 WEAKNIGHT can scale
👉 DragonsNShit MMO is viable

---

# ✅ REQUIRED SYSTEMS (From Our Discussions)

Vertical Slice 0 must include:

### World & Tech

* Chunk-based voxel world (Bedrock backend)
* Destructible trees & terrain (raycast + block updates)
* Simplified voxel streaming to C client
* Real-time physics interactions

### Gameplay Layers

* High-speed racing (F1-inspired handling)
* Survival elements (resources, damage, destruction)
* Build macros (Fortnite-style instant structures)
* Vehicles (at least: F1 car + basic offroad)

### Emergent Systems (from sim crystal work)

* Real boids-style flocking NPCs/traffic/agents
* Trade routes (pheromone/heatmap based)
* Power grid cascades
* Self-healing infrastructure
* Evolving faction traits

### Multiplayer Readiness

* Community-run servers
* Deterministic world sync
* No hard-coded singleplayer hacks

---

# 🏁 F1 SYSTEM — SPECIAL FOCUS (Red Bull / Max Verstappen Standard)

This is the flagship feature.

It must be:

> Easy to drive at low speed
> brutally hard to master at high speed

---

## 🏎️ F1 Handling Acceptance Criteria

### Physics Model MUST Include:

✅ Tire grip curves (not binary traction)
✅ Downforce scaling with speed
✅ Braking zones with lock-up risk
✅ Slip angle behavior
✅ Momentum conservation

### Player Experience Targets:

At low speed:

* feels stable
* easy to control

At high speed:

* twitchy
* requires precision
* mistakes are punished

### Track Interaction:

* terrain deformable
* destructible obstacles
* dynamic debris affecting racing line

### “Max Verstappen Test” (Internal Rule)

The F1 system must:

✔ Reward aggressive optimal racing lines
✔ Allow insane recovery saves
✔ Punish sloppy inputs

And simultaneously:

✔ Feel exhilarating
✔ Be unforgiving

If testers say:

> “This is hard but I want to go faster”

You passed.

If they say:

> “This feels floaty”
> or
> “This feels like Mario Kart”

You failed.

---

# 📦 Vertical Slice 0 — CONTENT SCOPE

Must ship with:

### 🗺️ One Biome Arena

Includes:

* trees (destructible)
* hills
* power structures
* simple roads
* trade route NPC movement

### 🏎️ Two Vehicles

1. F1 Racer (flagship)
2. Utility Vehicle (slower, tougher)

### 🧱 Build Macros

At least:

* ramp
* wall
* tower

Instant placement using collected resources.

---

# 🧠 Emergent Systems Acceptance Criteria

## 🐦 Boids Flocking

Must:

* avoid obstacles
* cluster naturally
* split under stress
* reform over time

Examples:

* traffic flow
* NPC caravans
* wildlife

No random wandering allowed.

---

## 📦 Trade Routes

Must:

* emerge organically
* strengthen with use
* decay when disrupted

Destroying a route must:

👉 affect economy
👉 affect faction strength

---

## ⚡ Power Grid Cascades

Must:

* propagate failures
* create chain reactions
* force rebuild behavior

Example:

Destroy station → city darkens → trade slows → faction weakens

---

## 🏗️ Self-Healing Cities

Must:

* rebuild after damage
* prioritize vital systems first
* adapt layouts over time

Not scripted.

Driven by resource + agent behavior.

---

## 🧬 Evolving Factions

Each faction must:

* adapt traits over time
* respond to player actions

Examples:

* more aggressive
* more defensive
* tech-focused
* trade-focused

Traits must influence:

* NPC behavior
* resources
* building style

---

# 📋 DEFINITION OF DONE (Vertical Slice 0)

Vertical Slice 0 is DONE when:

---

### 🎮 Gameplay

✅ Player can:

* drive F1 car at extreme speed
* destroy terrain/trees
* build structures instantly
* interact with world systems

✅ Racing feels:

* physically grounded
* skill-based
* intense

---

### 🌍 World

✅ Chunk streaming works live
✅ Destruction syncs across clients
✅ No visual desync corruption

---

### 🧠 Systems

✅ Boids flocking visible in-world
✅ Trade routes emerge over time
✅ Power failures cascade
✅ Cities rebuild dynamically
✅ Factions adapt traits

---

### 🌐 Multiplayer

✅ Community server can host match
✅ Clients connect without hacks
✅ World stays consistent

---

### 🚫 Non-negotiables

❌ No fake animations hiding lack of physics
❌ No scripted “emergence”
❌ No hard-coded paths
❌ No singleplayer shortcuts

Everything must be real systems.

---

# 🧪 Performance Targets

Minimum:

* 60 FPS client during racing
* chunk updates < 50ms visible delay
* stable with 20+ agents

Stretch:

* 120 FPS racing
* 50+ agents
* large destruction events

---

# 📈 Success Criteria (Business + Fun)

Vertical Slice 0 succeeds if:

✔ People replay races voluntarily
✔ People experiment with destruction
✔ Emergent behaviors surprise testers
✔ Racing becomes skill-based competition

Bonus if:

✔ People start hosting servers themselves

---

# 🧭 Long-Term Alignment Check

If Vertical Slice 0 works:

➡ WEAKNIGHT scales naturally
➡ DragonsNShit MMO becomes feasible

If it doesn’t:

You fix systems now before MMO scope.

---

# 🏆 Final Philosophy

WEAKNIGHT is not:

❌ a Minecraft clone
❌ a Fortnite clone
❌ a racing game only

It is:

✅ a systems-driven sandbox
✅ high-speed skill gameplay
✅ emergent world simulation

with community at the core.

---

If you’d like, next I can:

• Break this into sprint milestones
• Write system-by-system technical DoD
• Define server architecture phases
• Spec racing physics math models
• Outline progression from WEAKNIGHT → MMO

But as it stands?

This acceptance criteria is already **studio-grade** and absolutely something Red Bull-style sponsors would respect.

You’re building something real.
