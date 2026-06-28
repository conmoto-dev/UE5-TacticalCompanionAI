# UE5-TacticalCompanionAI

> A scalable C++ companion AI framework for Action RPGs, focused on natural-feeling party movement and positioning comparable to *Granblue Fantasy: Relink* and *Arknights: Endfield*.

## 🎬 Demo

*[Video/GIF embeds will be added as milestones complete]*

---

## ✨ Implemented Features

**Follow formation (out-of-combat)**
- **3-layer architecture** (Manager / Controller / Character) with a role-agnostic character class
- **Designer-driven formation data** via `UPrimaryDataAsset` — new formations require no code rebuild
- **Automatic V ↔ I formation switching** based on NavMesh raycast corridor width, with hysteresis
- **Environment-aware slot adjustment** — slope-aware Z correction, NavMesh-first validation, wall sliding as fallback
- **Spring-based gap scaling** + **quaternion delayed rotation** — the formation breathes with leader speed and turns with weight
- **Per-slot distance-based cache invalidation** — slots recompute only when the player moves away, so they don't chase the player during passage
- **Per-formation Yield Strategy** — abstract `UYieldStrategy` with `_Standard` / `_None` variants, designer-selectable via `Instanced` UPROPERTY, decoupled from component type via `IYieldContextProvider`
- **Hungarian-based slot reassignment** on sustained stop (O(N³) optimal matching; skipped while yielding)
- **Detour Crowd avoidance** (migrated from RVO) with a controller-decided role system — companions avoid the player and each other via dynamic priority groups
- **Tactical traversal** — companions jump *up* ledges along the path via projected-arc launch + Bézier approach steering (down-ledge handling in design; see Open Problems)

**Battle formation (in-combat)**
- **Mode switching** (Follow ↔ Battle) driven by the manager on perceived-enemy distance, with hysteresis
- **Target-relative slot placement** — slots anchor to the engaged target instead of the leader, with role-based grouping (melee vs. ranged) via `GameplayTag`
- **Two assignment policies** — set-based (melee arc: assign once on entry, then hold) and per-member (ranged: each unit commits independently)
- **Decision/execution split for ranged repositioning** — an influence-map oracle decides *when* to relocate (commit gate); locomotion handles the actual movement. Eliminates mid-move stalls and frame-rotation twitch
- **Omnidirectional influence-map positioning** for ranged units — no fixed front axis; candidates are scored 360° around the target by threat, ally proximity, and crowding

**Enemy initial placement (spawn-time)**
- **Composite formation built in place** — each `AEnemySpawner` holds an *inline* `FEnemyCompositeFormation`; designers add sub-formations (front row, back row…) with `+ / -` right in the Details panel and assign an enemy list + shape per row. Deliberately not a reusable preset asset — the goal was per-spawn-point editing, not reuse
- **Data-driven placement shapes** — `Line` / `Circle` / `Arc` / `Scatter` as swappable `Instanced` strategies under `UEnemySubFormationBase`; the contract is one `BuildSlots` call (`BlueprintNativeEvent`, so new shapes can be added in BP without a C++ rebuild — reusing the same strategy pattern as the ally formations)
- **Responsibility split (ideal position vs. resolution)** — placement shapes compute *only the ideal slots* and know nothing of the world; wall/obstacle/NavMesh resolution is handled in one place by the spawner. Adding a new shape stays pure position math — collision handling comes for free
- **Never force-spawns inside a wall** — if a slot is blocked, the spawner searches outward in expanding rings and (optionally) projects onto the NavMesh; if no safe spot is found it *gives up and logs* rather than burying an enemy in geometry
- **Author-and-verify loop in-editor** — spawn/clear are `CallInEditor`; outside PIE, previews spawn `RF_Transient` (no level pollution) and draw resolved positions as debug. Build → see → adjust, right in the viewport

---

## 🎯 Key Design Decisions

These are the decisions about *how responsibility is divided across the system* — the parts that show how it's reasoned about, not how any one feature is coded. Implementation-level choices (e.g. specific avoidance or traversal mechanics) live in the linked ADRs. Full rationale, rejected alternatives, and known limits for each are in **[`Docs/ADR/`](./Docs/ADR/)**.

### Why a separate PartyManager actor instead of attaching to a character?

The party system's lifetime must not be coupled to any single character's. If the formation manager lived on a Character component, the system would collapse when that character dies or is swapped out. By extracting `APartyManager` as a standalone actor, leader swap and member death become trivial state changes rather than system rebuilds — and the same skeleton can later host different formation components for enemy mobs. The manager owns *who is in the party and what mode is active*; components decide *how to behave within that mode*, and read the manager without ever writing to each other.

### Why Yield is a formation-level system, not per-character

A per-character "everyone avoids everyone" model is more flexible, but it has no shared context: companions avoiding the same player pile into the same spot, and members of one formation yield to each other and dissolve the formation. There's no center to resolve "you go left, you go right." Making Yield a formation-level responsibility accepts a deliberate limit (companions don't yield to unrelated NPCs) in exchange for coherent group behavior — which matches the genre (Relink/Endfield only yield within the player's party). *Deciding what a system deliberately won't do is itself design.*

### Why Yield uses the Strategy pattern, and why the strategy is stateless

New formations should add positioning behavior without editing existing code (OCP), and designers should pick behavior from data — so Yield is an `Instanced` Strategy on the formation asset, not a runtime-swapped object. It's stateless on purpose: Unreal DataAssets are flyweights, so two components sharing one asset share the strategy instance. If the strategy held per-slot state (yield flags, timers), those components would corrupt each other. All per-slot state lives on the component; the strategy only computes. → [ADR-0002](./Docs/ADR/0002-slot_generation_abstraction.MD)

### Why combat position is a "Home" (reference point), not a fixed slot

In combat, a companion's final position is shaped by many behaviors (attacking, dodging, repositioning), so a single authored slot can't be "where it must stand." The slot is reframed as a *Home* — a reference point a companion converges toward when it has nothing more urgent to do. Attacks and dodges don't delete the Home; they just outrank it temporarily. This keeps one consistent anchor to return to, instead of scattering exception logic ("if attacking, skip") across the formation code. → architecture overview in [`Docs/ADR/`](./Docs/ADR/)

### Why "deciding when to move" is split from "deciding where to move"

Running the influence map every frame turned an evaluator into a per-frame controller — companions stalled mid-move (re-picking ~their current spot) and got knocked off their position when the player orbited. The fix separates the two: a **commit gate** decides *when* a unit is allowed to relocate (first placement / out of attack range / threat too close), and only then is a new position computed and handed to locomotion. Between triggers, the position is locked. The same commit model becomes the seed for a later movement-arbitration layer. → [ADR-0003](./Docs/ADR/0003-battleformation_decision_execution_split.MD)

### Why ranged positioning decides nothing about direction — the score does

The first ranged-placement approach picked a front-facing direction and laid candidates in a narrow fan around it. That premise was the bottleneck: it left flanks and rear out of the candidate set entirely, and whatever the axis was anchored to (player or enemy centroid) made the whole candidate field rotate or land in empty space. So direction was removed from candidate *generation*: candidates are spread 360° around the target, and *every* judgment about "which spot is good" is delegated to independent scores (threat, ally proximity, crowding). No spot is special by construction — the score decides. Crowding finally disperses companions because there are spots on all sides to disperse *into*. → [ADR-0004](./Docs/ADR/0004-rangedsafe_omnidirectional_influence_map.MD)

### Why StateTree (not Behavior Tree) for the planned mode layer

*(Not yet implemented — rationale recorded ahead of time.)* Mode selection is a *commit-to-a-state* problem: enter Battle, stay there, transition only when conditions change. A Behavior Tree re-traverses from the root every tick to pick a leaf — great for reactive "what do I do right now," wrong for committed modes (you end up faking states with blackboard flags and re-entry guards). StateTree combines a Selector with explicit States/Transitions, with hysteresis and priority built in. The current `if/else` mode switch is a deliberate stepping stone toward it — built only once combat creates a genuine second state and transition.

### Why enemy placement is built in the spawner, not chosen from a reusable preset

The obvious shape is a preset asset (`Line+Circle`, etc.) that each spawn point references. Presets are strong for *reuse* — but enemy placement isn't a reuse problem. Spawning varied formations across different points is part of what makes encounters interesting, so reuse isn't automatically the right answer here; and the people authoring it are designers and planners, not programmers. The moment you want each point slightly different, a shared preset drags in duplication, override rules, variants, and sync between the asset and the spawner. *Reuse* and *in-place editing* are different goals, and this called for the latter — so the composite formation is inline data the spawner owns, edited and read in one place. (Placement intent is also expressed as data by the designer, never inferred at runtime from proximity — consistent with the enemy-group direction in [ADR-0006](./Docs/ADR/0006-enemy_group_combat_architecture.MD).) → [ADR-0007](./Docs/ADR/0007-enemy_spawn_composite_formation.MD)

### Why placement shapes compute only ideal positions, with resolution pulled up to the spawner

If `Line` / `Circle` resolved their own wall checks and NavMesh projection, every shape would have to know about world collision and the navigation system — and the *same* resolution code would be duplicated across all of them, re-implemented every time a shape is added. So shapes stay pure (ideal slots only) and the spawner owns wall/obstacle/NavMesh resolution in one place. Adding a shape becomes pure position math; collision handling follows for free. *Deciding what a component deliberately won't know is itself design.* → [ADR-0007](./Docs/ADR/0007-enemy_spawn_composite_formation.MD)

<details>
<summary>Other decisions (folded)</summary>

- **Avoidance role decided by the controller, not the character** — Detour treats AIController-driven pawns as crowd members automatically, but the player isn't on that pipeline and must be registered manually; the controller is the single entry point that sets each pawn's avoidance role (Leader / Normal / Yielding) through a small interface. *Implementation-level — full write-up under [`Docs/ADR/`](./Docs/ADR/) / planned.*
- **Interface early, base class late** — `ITacticalAvoidanceController` is extracted now (near-free, lets a future enemy controller extend without touching existing code); a shared controller *base class* is deferred until there's actual shared implementation to pull up, rather than an empty shell.
- **Strategy reuse across components** — strategies depend on `IYieldContextProvider`, not a concrete component type, so the same strategy works when a new formation component (e.g. battle) becomes the caller.

</details>

---

## 🧩 Open Problems (Currently Investigating)

Real game-feel issues observed during development. Full detail — why each occurs and the trade-offs of each candidate fix — is tracked in the Korean roadmap notes.

- **Down-ledge traversal: jump vs. natural fall** — up-ledge jumping works; descending is a different category (a player walks off a ledge rather than jumping). Leaning toward asymmetric handling: up = jump, down = walk-off.
- **Companions walk off cliffs when pushed by the player** — default physics pushing has no walkable-area constraint; needs NavMesh edge detection + outward force projection.
- **Slot cache distance threshold is V-formation-biased** — a single component-level value clusters companions in I-formation; should move to `FormationDataAsset` for per-formation tuning.

---

## 🚧 Roadmap

**Completed**
- 3-layer architecture, V/I formations, NavMesh-aware adjustment, spring gap, quaternion rotation, auto V↔I switching
- Hungarian slot reassignment on stop; per-slot distance-based cache invalidation
- Yield Strategy decoupling; Hungarian-Yield collision fix
- RVO → Detour Crowd migration; controller-decided avoidance role system (player-as-agent, yield↔crowd sync)
- Up-ledge tactical traversal (arc launch + Bézier steering)
- Battle formation: mode switching, target-relative placement, set-based vs. per-member assignment
- Decision/execution split (commit gate) and omnidirectional influence-map positioning for ranged units
- Enemy initial placement: in-spawner composite formation, Line/Circle/Arc/Scatter shapes, wall/NavMesh spawn resolution

**Next**
- Down-ledge traversal (see Open Problems)
- Move `SlotCacheUpdateDistance` to `FormationDataAsset`
- Re-evaluation during ongoing Yield; leader-swap implementation (handlers are already in place)

**Following** (ordered)
- **StateTree decision layer** — migrate mode selection out of the component into StateTree (rationale under Key Design Decisions). The current `if/else` is a deliberate stepping stone.
- **Movement-intent arbitration** — generalize the per-unit commit model into a single resolver that mediates competing movement intents (attack / dodge / traversal / home) by priority and interruptibility.
- **Enemy group combat — decoupling strategy from type** — group combat policy isn't fixed to monster type; individual temperament (engagement range, flee threshold) stays on the monster while group formation (encircle / spread) is placement data. Built on top of spawn-time placement (done), adding slot-anchor memory → target/anchor repositioning → in-combat behavior. Order: combat → StateTree → this. Key contract: the group strategy emits intent (slot/target/posture) only, never calling GAS abilities directly. → [ADR-0006](./Docs/ADR/0006-enemy_group_combat_architecture.MD) / [ADR-0007](./Docs/ADR/0007-enemy_spawn_composite_formation.MD)
- **Enemy formation (Flock-based)** — leaderless group movement, reusing infrastructure via the shared character base.
- Lower priority: `UYieldStrategy_Narrow` (corridor flip); NavLink-aware jump; NavMesh edge avoidance; skill chaining.

---

## ⚙️ Tech Stack

- **Engine**: Unreal Engine 5.7+
- **Language**: C++ with Blueprint integration
- **Patterns**: Component-based, Manager-driven, Pawn-Controller separation, Strategy pattern + UInterface for cross-component reuse, influence-map scoring with a commit gate

---

🇯🇵 [日本語版](./README.MD) | 🇰🇷 [개인 로드맵](./README.ko.md)