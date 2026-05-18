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
- **Per-slot distance-based cache invalidation** — slot positions only recompute when the player moves away, preventing slots from chasing the player during passage
- **Per-formation Yield Strategy** — abstract `UYieldStrategy` with concrete `_Standard` (cone + side-step + projected backward) and `_None` variants; designer-selectable per formation via `Instanced` UPROPERTY; decoupled from component type via `IYieldContextProvider`
- **Hungarian-based slot reassignment on sustained stop** — optimal matching with O(N³) algorithm; skipped while any slot is yielding to prevent destination swap mid-flight

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

### Why Yield is a formation-level system, not a per-character one

A more permissive design would give every character its own yield logic — each one checking nearby actors and stepping aside independently. This would let any two characters yield to each other regardless of party, formation, or faction.

That design was rejected. With per-character yield, N×M proximity checks scale poorly, and worse, characters lack any shared context: companions yielding to the same player would crowd the same evasion spot, and characters inside the same formation would yield to *each other*, dissolving the formation's intended shape. There is no central place to detect a deadlock or to coordinate "everyone step aside differently."

Putting yield on `FormationFollowComponent` accepts a real limitation in exchange for these capabilities: **characters in different formations cannot yield to each other**. A party companion will not step aside for an unrelated NPC walking by, because the NPC isn't part of any formation the component manages.

This trade-off is deliberate. In *Granblue Fantasy: Relink* and *Arknights: Endfield*, only the player's party yields — enemies stand their ground or push forward, which is the genre-appropriate behavior anyway. Cross-formation evasion, when it matters, belongs to a different system entirely (Detour Crowd / RVO), not to the yield system. A design that tries to express *everything* usually expresses each thing poorly; choosing what the system *doesn't* do is part of the design.

### Why Yield uses the Strategy pattern

The Yield logic for V-formation (wide path, step aside + back) is not the same algorithm as Yield logic for I-formation (narrow corridor, hug the wall, optionally flip the formation after the player passes). These can't be unified through parameters — they're structurally different behaviors.

`UYieldStrategy` is an abstract base; concrete strategies (`_Standard`, `_None`, future `_Narrow`) implement the actual algorithms. `FormationDataAsset` holds the strategy as an `Instanced` `UPROPERTY`, so designers pick which strategy each formation uses, with that strategy's parameters appearing inline in the asset's detail panel. New yield algorithms become new classes — no existing code changes.

`IYieldContextProvider` keeps the strategy decoupled from any specific component type. The component implements the interface and passes itself as context; the strategy never sees `UFormationFollowComponent` by name. When a future `BattleFormationComponent` or `EnemyGroupComponent` is added, the same strategy classes work unchanged as long as the new component implements the interface.

### Why per-slot state lives on the component, not the strategy

Unreal `DataAsset` follows the Flyweight pattern: when multiple components reference the same asset, they share a single in-memory instance, including any `Instanced` members like the strategy. If the strategy held per-slot state (yielding flags, timers, target locations), two components sharing the same formation asset would also share — and corrupt — each other's yield state.

The strategy is therefore stateless: it provides judgment and calculation, nothing else. All per-slot state (`SlotYieldStates`, `SlotYieldDelayTimers`, `CachedYieldLocations`) lives on the component, which owns its own copy regardless of which asset it points to. The component drives the state machine; the strategy only answers questions about it. This is what makes asset sharing safe in any future multi-group scenario without changing the strategy hierarchy.

<details>
<summary>More: hysteresis, async LineTrace deferral, etc.</summary>

**Hysteresis for environment-based switching**: A single width threshold causes the formation to flicker near the boundary. Two thresholds (`NarrowThreshold=300`, `WideThreshold=500`) ensure transitions only fire when fully crossing into the opposite regime.

**Async LineTrace deferred**: With 3 followers and distance-based polling (50cm threshold), spatial query cost is negligible. Async would add callback complexity and stale-data handling with no measured performance benefit. Reconsidered when profiling shows actual bottlenecks (~30+ agents).

**Yield uses character facing, not velocity**: Cone detection uses the target's body orientation rather than velocity vector. The game decouples camera direction from character facing, so a stationary player turning to look at a companion still triggers yield. Velocity-based detection would miss the "player about to step forward" moment.

</details>

### Why StateTree is the planned next step for formation decision

Environment-based V/I switching currently lives inside `FormationFollowComponent`
as a simple width measurement + hysteresis if/else. This works for two formations
in the peacetime mode, but the pattern doesn't scale: each new formation (combat
surround, hazard avoidance, jump traversal) would compound the measurement and
branching logic in a single function, mixing decision and behavior in the same place.

StateTree (UE 5.7+) is the planned next step because it provides exactly the
separation this system needs:
- Each state (Wide / Narrow / Combat / Hazard ...) owns its own transition conditions
- New formations attach as independent state nodes without touching existing ones
- Composite conditions (e.g., "narrow corridor *and* sustained for 0.5s *and* not
  in combat") become explicit tree nodes instead of nested if/else
- Built-in support for hysteresis, cooldowns, and state priorities

The current if/else acts as a deliberate stepping stone: it surfaces the actual
shape of formation decisions and makes the StateTree migration a refactor of
*known logic*, not a leap into unfamiliar territory.

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

### 4. Slot cache distance threshold is V-formation-biased

The per-slot distance threshold (`SlotCacheUpdateDistance`) currently sits on the component as a single value tuned for V-formation, where slots sit far from the leader. In I-formation, where slots line up close behind the leader (-150, -300, -450), the threshold prevents near slots from updating while far slots do — resulting in companions clustering at one point.

- **Direction**: Move the threshold to `FormationDataAsset` for per-formation tuning. Long-term, recognize that update policy itself may be formation-dependent (could become another Strategy in the same pattern as Yield).

---

## 🚧 Roadmap

**Completed**
- 3-layer architecture, V/I formations, NavMesh-aware environment adjustment, spring-based gap, quaternion rotation, automatic V↔I switching
- Hungarian-based slot reassignment on sustained stop
- Per-slot distance-based cache invalidation
- Yield Strategy decoupling (per-formation algorithm selection via `Instanced` UPROPERTY)
- Hungarian-Yield collision fix (skip reassignment while any slot is yielding)

**Next**
- Move `SlotCacheUpdateDistance` to `FormationDataAsset` for per-formation tuning
- Re-evaluation during ongoing Yield (currently a yielding slot doesn't recompute its target if the player keeps approaching)
- Camera/input refactor from Character to PlayerController (true player/companion separation)

**Following**
- **StateTree-based decision layer** — migrate environment measurement and
  formation selection out of FormationFollowComponent; enable composite
  transition conditions; foundation for combat/hazard/yield mode integration
- `UYieldStrategy_Narrow` — I-formation "corridor flip" algorithm (wall-hug + post-pass rearrange)
- `ATacticalCharacterBase` abstraction so Strategies can apply to enemy/NPC groups
- NavMesh edge avoidance (cliff/hazard handling)
- NavLink-aware jump
- Detour Crowd Manager integration
- Enemy formation system (Flock-based)
- Player skill → Companion skill chaining

---

## ⚙️ Tech Stack

- **Engine**: Unreal Engine 5.7+
- **Language**: C++ with Blueprint integration
- **Patterns**: Component-based, Manager-driven, Pawn-Controller separation, Strategy pattern with UInterface for cross-component reuse

## 📁 Project Structure
 ```
Source/TacticalAI/
├── (Root)              Template-generated classes
├── Characters/         APartyCharacter
├── Controllers/        AI / Player controllers
├── Party/              APartyManager
├── Data/               UFormationDataAsset
├── AI/Components/      UFormationFollowComponent
└── AI/Strategies/      UYieldStrategy + IYieldContextProvider
  ```

---

🇯🇵 [日本語版](./README.ja.md) | 🇰🇷 [개인 로드맵](./README.ko.md)