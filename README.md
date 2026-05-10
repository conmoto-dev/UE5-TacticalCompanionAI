# UE5-TacticalCompanionAI

> A scalable C++ companion AI framework for Action RPGs, focused on natural-feeling party movement comparable to *Granblue Fantasy: Relink* and *Arknights: Endfield*.

## 🎬 Demo

*[Video/GIF embeds will be added as milestones complete]*

---

## 🎯 Key Design Decisions

### Why a separate PartyManager actor instead of attaching to a character?

The party system's lifetime must not be coupled to any single character's lifetime. If the formation manager lived on a Character component, the system would collapse when that character dies or is swapped out for another playable member. By extracting `APartyManager` as a standalone actor, leader swap and member death become trivial state changes rather than system rebuilds.

This pattern also generalizes: future enemy mob systems can use the same `AGroupManager` skeleton with a different formation component, sharing infrastructure between ally and enemy AI.

### Why Sphere Sweep instead of Line Trace for wall detection?

Line Trace has zero thickness, which produces false negatives in narrow gaps that a character capsule cannot actually traverse. The trace passes through, but the character would collide. Sphere Sweep with the character's capsule radius matches the actual movement footprint, ensuring the wall sliding correction triggers when it should.

### Why VectorPlaneProject for sliding correction instead of formation-wide compression?

When a single slot is blocked by a wall, compressing the entire formation makes all companions look mechanically tied to one obstacle. Instead, only the blocked slot is projected onto the wall's normal plane via `VectorPlaneProject`, so the affected companion slides naturally along the wall while others maintain the V-formation. This isolates the visual disturbance to where it actually originates.

### Why hold off on async LineTrace despite the "modern" appeal?

With 3 followers and distance-based polling (50cm threshold), the spatial query cost is negligible (~360 traces/sec on Tick at 60fps). Async would introduce callback complexity, stale-data handling, and ticket-based defensive code without any measured performance benefit. Async will be reconsidered when profiling indicates an actual bottleneck (e.g., >30 agents, large-scale crowd scenarios).

---

## 🧩 Open Problems (Currently Investigating)

These are real game-feel issues observed during development. Each entry documents *why* the problem occurs and *what trade-offs* each solution involves — the focus is on understanding before implementation.

### 1. Companions walk off cliffs when pushed by player

In games like *Endfield*, when a player pushes a companion via collision, the companion only gets shoved into walkable areas — never off ledges. Default physics-based pushing has no such constraint.

- **Hypothesis**: clamp pushed positions to nearest NavMesh point each frame, OR apply outward force when companions are within edge proximity threshold
- **Direction**: NavMesh edge detection + force vector projection along the edge tangent

### 2. Naive jump following — companions jump in mid-air

If companions blindly mimic the leader's jump input, they jump over flat ground when there's nothing to clear. Real games trigger jump *only when the path actually requires it*.

- **Hypothesis**: NavLink-marked transitions for designed jumps, plus path lookahead for procedural gap detection
- **Direction**: NavLink integration as primary, path lookahead as fallback for unmarked terrain

### 3. RVO Avoidance produces "shuffling" between agents with similar goals

When multiple companions need to occupy nearby slots after a sharp turn or formation switch, RVO causes oscillating side-steps instead of clean path resolution. RVO is reactive, not predictive — it lacks group awareness.

- **Hypothesis**: Detour Crowd Manager (UE5 native) provides predictive avoidance with group context; Hungarian algorithm for optimal slot reassignment minimizes total displacement during transitions
- **Direction**: Detour Crowd as base layer, Hungarian matching applied during formation transitions

### 4. Companion blocking player movement

When a player walks into a companion's path, both characters get stuck "rubbing" against each other. Making characters pass through ghost-style breaks game feel; tuning RVO has limits.

- **Hypothesis**: Detour Crowd's group-aware avoidance helps, plus per-companion "yield" behavior — companions detect player approach within a small radius and step aside
- **Direction**: Detour Crowd integration + lightweight yield logic in companion BT/StateTree

---

## ✨ Implemented Features

- **3-layer architecture** (Manager / Controller / Character) with role-agnostic character class
- **Mathematical formation positioning** with foot-based reference (avoids Z-axis floating)
- **Environment-aware wall sliding** via Sphere Sweep + VectorPlaneProject
- **NavMesh projection fallback** with leader-tow safety net for unreachable slots
- **Spring-based gap scaling** (`FloatSpringInterp`) — formation expands/contracts with leader speed
- **Quaternion-based delayed rotation** — followers heavy-follow rotation, avoiding mechanical snap
- **Distance + rotation-aware cache invalidation** — heavy queries only when needed

---

## 🚧 Roadmap

**In Progress**
- Camera collision channel refinement
- `UFormationDataAsset` for designer-driven formation definitions

**Planned**
- I-shape formation with corridor detection (V → I auto-transition)
- StateTree-based decision layer (combat / non-combat / yield)
- Enemy formation system (Flock-based)
- Player skill → Companion skill chaining

---

## ⚙️ Tech Stack

- **Engine**: Unreal Engine 5.7+
- **Language**: C++ with Blueprint integration
- **Patterns**: Component-based, Manager-driven, Pawn-Controller separation

## 📁 Project Structure
Source/TacticalAI/
├── (Root)              Template-generated classes
├── Characters/         APartyCharacter
├── Controllers/        AI / Player controllers
├── Party/              APartyManager
└── AI/Components/      UFormationFollowComponent

---

🇺🇸 [English](./README.md) | 🇰🇷 [개인 로드맵](./README.ko.md)