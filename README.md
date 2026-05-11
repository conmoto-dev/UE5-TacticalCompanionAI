# UE5-TacticalCompanionAI

> A scalable C++ companion AI framework for Action RPGs, focused on natural-feeling party movement comparable to *Granblue Fantasy: Relink* and *Arknights: Endfield*.

## 🎬 Demo

*[Video/GIF embeds will be added as milestones complete]*

---

## ✨ Implemented Features

- **3-layer architecture** (Manager / Controller / Character) with role-agnostic character class
- **Designer-driven formation data** via `UPrimaryDataAsset` — new formations require no code rebuild
- **Automatic V ↔ I formation switching** based on NavMesh raycast corridor width, with hysteresis
- **Mathematical formation positioning** with foot-based reference (avoids Z-axis floating)
- **Environment-aware slot adjustment** — slope-aware Z correction, NavMesh-first validation, wall sliding as fallback
- **Spring-based gap scaling** (`FloatSpringInterp`) — formation expands/contracts with leader speed
- **Quaternion-based delayed rotation** — followers heavy-follow rotation, avoiding mechanical snap
- **Distance + rotation-aware cache invalidation** — heavy queries only when needed

---

## 🎯 Key Design Decisions

### Why a separate PartyManager actor instead of attaching to a character?

The party system's lifetime must not be coupled to any single character's lifetime. If the formation manager lived on a Character component, the system would collapse when that character dies or is swapped out. By extracting `APartyManager` as a standalone actor, leader swap and member death become trivial state changes rather than system rebuilds. The same `AGroupManager` skeleton can later host different formation components for enemy mobs, sharing infrastructure between ally and enemy AI.

### Why Sphere Sweep instead of Line Trace for wall detection?

Line Trace has zero thickness, which produces false negatives in narrow gaps that a character capsule cannot actually traverse. The trace passes through, but the character would collide. Sphere Sweep with the character's capsule radius matches the actual movement footprint, ensuring wall sliding correction triggers when it should.

### Why NavMesh as the *primary truth* in environment adjustment?

An earlier flow ran *Wall Detect → Slide → NavMesh* in sequence — but sliding results that landed off NavMesh were used as-is, causing companions to target unreachable positions on slope-shaped non-walkable areas. The flow was restructured so NavMesh projection becomes the *primary validity check*, with wall sliding demoted to a *recovery tool* invoked only when projection fails. Each helper has a single responsibility, and `AdjustLocationForEnvironment` reads top-to-bottom as a four-step orchestration.

### Why layered decisions: Manager picks the *mode*, Component decides *within* the mode?

Environment-based V/I switching is a decision *inside the peacetime mode*, so it lives in `FormationFollowComponent`. When combat mode is introduced, `APartyManager` will swap the active component rather than fight the component for the same decision. Keeping abstraction levels from overlapping is what allows new modes to be added without touching existing ones.

<details>
<summary>More: hysteresis, async LineTrace deferral, etc.</summary>

**Hysteresis for environment-based switching**: A single width threshold causes the formation to flicker near the boundary. Two thresholds (`NarrowThreshold=300`, `WideThreshold=500`) ensure transitions only fire when fully crossing into the opposite regime.

**Async LineTrace deferred**: With 3 followers and distance-based polling (50cm threshold), spatial query cost is negligible. Async would add callback complexity and stale-data handling with no measured performance benefit. Reconsidered when profiling shows actual bottlenecks (~30+ agents).

</details>

---

## 🧩 Open Problems (Currently Investigating)

These are real game-feel issues observed during development. Each entry documents *why* the problem occurs and *what trade-offs* each solution involves.

### 1. Companions walk off cliffs when pushed by player

In games like *Endfield*, when a player pushes a companion via collision, the companion only gets shoved into walkable areas — never off ledges. Default physics-based pushing has no such constraint.

- **Direction**: NavMesh edge detection + outward force projection along the edge tangent

### 2. Naive jump following — companions jump in mid-air

If companions blindly mimic the leader's jump input, they jump over flat ground when there's nothing to clear. Real games trigger jump *only when the path actually requires it*.

- **Direction**: NavLink-marked transitions as primary, path lookahead as fallback for unmarked terrain

### 3. RVO can't resolve grouped-agent crowding cleanly

Default RVO is reactive and lacks group awareness. After sharp turns or formation switches, companions oscillate against each other instead of finding distinct paths. Player-blocking companions cause similar "shuffling".

- **Direction**: Detour Crowd Manager (UE5 native) for predictive group-aware avoidance, plus Hungarian algorithm for optimal slot reassignment during formation transitions

---

## 🚧 Roadmap

**Next (Week 2)**
- Hungarian algorithm for slot reassignment on formation switch
- Refactor camera/input from Character into PlayerController (true player/companion separation)

**Following**
- StateTree-based decision layer (peace / combat / yield)
- NavMesh edge avoidance (cliff/hazard handling, see Open Problems #1)
- NavLink-aware jump (see Open Problems #2)
- Detour Crowd Manager integration (see Open Problems #3)
- Enemy formation system (Flock-based)
- Player skill → Companion skill chaining

---

## ⚙️ Tech Stack

- **Engine**: Unreal Engine 5.7+
- **Language**: C++ with Blueprint integration
- **Patterns**: Component-based, Manager-driven, Pawn-Controller separation

## 📁 Project Structure
 ```
 Source/TacticalAI/
├── (Root)              Template-generated classes
├── Characters/         APartyCharacter
├── Controllers/        AI / Player controllers
├── Party/              APartyManager
├── Data/               UFormationDataAsset
└── AI/Components/      UFormationFollowComponent
  ```

---

🇯🇵 [日本語版](./README.ja.md) | 🇰🇷 [개인 로드맵](./README.ko.md)