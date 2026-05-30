# UE5-TacticalCompanionAI

> A scalable C++ companion AI framework for Action RPGs, focused on natural-feeling party movement comparable to *Granblue Fantasy: Relink* and *Arknights: Endfield*.

## 🎬 Demo

*[Video/GIF embeds will be added as milestones complete]*

---

## ✨ Implemented Features

- **3-layer architecture** (Manager / Controller / Character) with a role-agnostic character class
- **Designer-driven formation data** via `UPrimaryDataAsset` — new formations require no code rebuild
- **Automatic V ↔ I formation switching** based on NavMesh raycast corridor width, with hysteresis
- **Environment-aware slot adjustment** — slope-aware Z correction, NavMesh-first validation, wall sliding as fallback
- **Spring-based gap scaling** + **quaternion delayed rotation** — formation breathes with leader speed and turns with weight
- **Per-slot distance-based cache invalidation** — slots recompute only when the player moves away, so they don't chase the player during passage
- **Per-formation Yield Strategy** — abstract `UYieldStrategy` with `_Standard` / `_None` variants, designer-selectable via `Instanced` UPROPERTY, decoupled from component type via `IYieldContextProvider`
- **Hungarian-based slot reassignment** on sustained stop (O(N³) optimal matching; skipped while yielding)
- **Detour Crowd avoidance** (migrated from RVO) with a controller-decided role system — companions avoid the player and each other via dynamic priority groups
- **Tactical traversal** — companions jump *up* ledges along the path via projected-arc launch + Bézier approach steering (down-ledge handling in design; see Open Problems)

---

## 🎯 Key Design Decisions

The full rationale for every decision would be too long to read; below are the few that best show how the system is reasoned about. Others are folded under `<details>`.

### Why a separate PartyManager actor instead of attaching to a character?

The party system's lifetime must not be coupled to any single character's. If the formation manager lived on a Character component, the system would collapse when that character dies or is swapped out. By extracting `APartyManager` as a standalone actor, leader swap and member death become trivial state changes rather than system rebuilds — the same skeleton can later host different formation components for enemy mobs.

### Why Detour Crowd avoidance is decided by the *controller*, not the character

The migration from RVO to Detour Crowd exposed a structural asymmetry. Detour automatically treats anything moving via an AIController as a crowd member — but the player isn't on that pipeline, so it must be registered manually as a crowd agent. The question was *where* that registration and the avoidance role (Leader / Normal / Yielding) get decided.

The answer is the controller, reached through a small interface (`ITacticalAvoidanceController`). Possession is the single entry point: an `OnPossess` handler sets the role, so the player's pawn becomes a crowd obstacle and AI pawns become normal followers — and leader swap (= re-possession) reuses the exact same path with no extra code. The character stays passive; it never decides its own avoidance role. External systems (e.g. the formation's yield logic) command a role through the interface without knowing whether the controller is player, AI, or a future enemy type.

<details>
<summary>Why an interface on the controller, rather than a hub component on the pawn</summary>

A tempting alternative is a single "avoidance hub" component on the pawn that routes commands to either the player-agent component or the AI's crowd-following component. It was rejected: the crowd-following component is owned by the *controller* (it's the PathFollowing component), while the player-agent component is owned by the *pawn*. A hub on the pawn would have to reach across the ownership boundary into the controller's internals and branch on `if (player) / else (AI)` at runtime — pushing the same branch down rather than removing it. The interface resolves the branch through controller polymorphism, and each controller touches only what it owns. Two edge cases sometimes cited for the hub (multiplayer state replication; non-pawn crowd participants like carts) turn out to be orthogonal — replication belongs on a pawn-side replicated variable regardless, and non-pawn participants are already covered by the component-based agent registration. Both extend on top of the interface rather than replacing it.

</details>

### Why Yield is a formation-level system, not per-character

A permissive design would give every character its own yield logic, letting any two characters step aside for each other. It was rejected: per-character yield scales as N×M proximity checks and, worse, lacks shared context — companions yielding to the same player crowd the same spot, and characters in one formation yield to *each other*, dissolving the shape. Putting yield on the formation component accepts one deliberate limitation — **characters in different formations can't yield to each other** — in exchange for a central place to coordinate. This matches the genre: in *Relink* and *Endfield* only the player's party yields; enemies hold their ground. Cross-formation evasion, when needed, belongs to Detour Crowd, not the yield system. Choosing what the system *doesn't* do is part of the design.

### Why Yield uses the Strategy pattern, and why the strategy is stateless

V-formation yield (step aside + back) and I-formation yield (hug the wall, flip after passing) are structurally different algorithms, not parameter variations. `UYieldStrategy` is an abstract base; concrete strategies become new classes with no existing-code changes, and `FormationDataAsset` holds one as an `Instanced` UPROPERTY so designers pick per formation. The strategy is kept **stateless** because Unreal `DataAsset` is a Flyweight — two components sharing one asset share its `Instanced` members, so any per-slot state on the strategy would be corrupted across components. All per-slot state lives on the component instead; the strategy only answers questions.

### Why StateTree (not Behavior Tree) for the planned mode layer

This is an internal-architecture decision — nothing about it is visible on screen — but it shapes how the whole decision layer scales, so the reasoning is recorded deliberately.

Mode selection (peacetime ↔ combat, later hazard) is fundamentally a *state* problem: the party commits to a mode and stays there until a transition condition fires. That is the opposite of how a Behavior Tree works. A BT re-traverses from the root every tick to find a suitable leaf — the right model for *reactive task selection* ("what action do I take right now"), the wrong model for *committed mode*. Unreal's own framing matches this: a state machine commits state selection as execution descends, whereas a behavior tree keeps searching for a leaf. Forcing committed modes into a BT means simulating state with blackboard flags and guarding every branch against re-entry — an implicit, fragile state machine scattered across decorators.

StateTree fits because it *combines* both models: the Selectors of a BT with the explicit States and Transitions of a state machine. That yields what mode selection needs and a BT doesn't:
- Each mode owns its enter conditions and transitions — a new mode (combat, hazard) attaches as an independent node without editing existing ones (OCP at the decision layer, mirroring how Yield strategies extend by addition).
- Composite transitions ("corridor narrow *and* sustained 0.5s *and* not in combat") are explicit tree nodes, not nested `if/else`.
- Hysteresis, cooldowns, and state priority are first-class instead of hand-rolled per branch.
- Utility-based selection (weights scaling with, say, health to pick *fight* vs *flee*) is built in — directly useful for the combat layer.

The current `if/else` V↔I switch is a deliberate stepping stone, not the destination: it surfaces the real shape of the decision so the migration is a refactor of *known* logic rather than a leap. **Crucially, the migration is gated on having more than one mode** — with only Idle/Follow today, a state machine would be ceremony around a single node. The right order is to build the combat formation *first* (so a genuine second state and a real transition exist), then introduce StateTree to manage the states that now actually exist. Adopting the tool before the problem is the over-engineering trap StateTree itself invites; knowing *when* it earns its place is part of the decision.

<details>
<summary>More: NavMesh-first adjustment, layered Manager/Component decisions, sphere sweep, hysteresis, facing-based cone</summary>

**NavMesh as the primary truth in environment adjustment**: An earlier flow ran *Wall Detect → Slide → NavMesh*, but slides landing off NavMesh were used as-is, sending companions toward unreachable spots. Restructured so NavMesh projection is the *primary validity check*, with wall sliding demoted to a recovery tool. `AdjustLocationForEnvironment` now reads top-to-bottom as a four-step orchestration, each helper single-responsibility.

**Manager picks the mode, Component decides within it**: V/I switching is a peacetime-internal decision, so it lives in the component. When combat mode arrives, the Manager swaps the active component rather than fighting it for the same decision — keeping abstraction levels from overlapping.

**Sphere sweep over line trace**: Line trace has zero thickness and passes through gaps a capsule can't, giving false negatives. Sphere sweep at capsule radius matches the real movement footprint.

**Hysteresis for switching**: A single width threshold flickers at the boundary; two thresholds (Narrow/Wide) only fire on fully crossing into the opposite regime.

**Yield uses facing, not velocity**: The game decouples camera from body facing, so a stationary player turning to look at a companion should still trigger yield — velocity-based detection would miss it.

</details>

---

## 🧩 Open Problems (Currently Investigating)

Real game-feel issues observed during development. Each documents *why* it occurs and *what trade-offs* the candidate solutions carry.

### 1. Down-ledge traversal: jump vs. natural fall

Up-ledge jumping works (arc launch + steering). Going *down* is a different category that an earlier attempt wrongly forced through the same takeoff-point math. The deeper question is design, not code: a player descends a ledge by just walking off it, not by jumping — so companions jumping down looks wrong. But "just walking off" is hard for AI because `MoveTo` only operates on NavMesh, and NavMesh ends at a ledge.

- **Trade-off**: NavLink-marked descents (engine-standard, robust, but authored per-ledge) vs. a "walk-off-ledge" state that hands the character to direct movement input past the NavMesh edge (dynamic, but needs edge detection and a brief off-mesh control window). Likely outcome: descent should *not* be a jump; up = jump, down = walk-off, asymmetric by design.

### 2. Companions walk off cliffs when pushed by the player

Default physics pushing has no walkable-area constraint; *Endfield* only shoves companions into walkable space.

- **Direction**: NavMesh edge detection + outward force projection along the edge tangent.

### 3. Slot cache distance threshold is V-formation-biased

`SlotCacheUpdateDistance` is a single component-level value tuned for V-formation (slots far from leader). In I-formation (slots close behind), it lets far slots update while near slots don't, clustering companions.

- **Direction**: move the threshold to `FormationDataAsset` for per-formation tuning; long-term the update policy itself may become a Strategy, like Yield.

> *Resolved:* grouped-agent crowding after turns/switches, previously listed here, was addressed by the RVO → Detour Crowd migration plus Hungarian reassignment.

---

## 🚧 Roadmap

**Completed**
- 3-layer architecture, V/I formations, NavMesh-aware adjustment, spring gap, quaternion rotation, auto V↔I switching
- Hungarian slot reassignment on stop; per-slot distance-based cache invalidation
- Yield Strategy decoupling; Hungarian-Yield collision fix
- RVO → Detour Crowd migration; controller-decided avoidance role system (player-as-agent, yield↔crowd sync)
- Up-ledge tactical traversal (arc launch + Bézier steering)

**Next**
- Down-ledge traversal (see Open Problem #1)
- Move `SlotCacheUpdateDistance` to `FormationDataAsset`
- Re-evaluation during ongoing Yield; leader-swap implementation (handlers are already in place)

**Following** (ordered)
- **Combat formation** — target-relative (vs. leader-relative) placement, role-constrained slots (tank front / healer back). Built *before* StateTree, because it creates the genuine second state and transition that make a state machine meaningful. A test of whether the existing system extends by addition, not modification.
- **StateTree decision layer** — once combat exists, migrate mode selection out of the component into StateTree (rationale under Key Design Decisions). The if/else is a deliberate stepping stone toward this.
- **Enemy formation (Flock-based)** — leaderless group movement, reusing infrastructure via `ATacticalCharacterBase`.
- Lower priority: `UYieldStrategy_Narrow` (corridor flip); NavLink-aware jump; NavMesh edge avoidance (cliff/hazard — deprioritized since the current test map rarely surfaces it); skill chaining.

---

## ⚙️ Tech Stack

- **Engine**: Unreal Engine 5.7+
- **Language**: C++ with Blueprint integration
- **Patterns**: Component-based, Manager-driven, Pawn-Controller separation, Strategy pattern + UInterface for cross-component reuse

## 📁 Project Structure
```
Source/TacticalAI/
├── Characters/    APartyCharacter
├── Controllers/   AI / Player controllers (avoidance role via ITacticalAvoidanceController)
├── Party/         APartyManager
├── Data/          UFormationDataAsset
├── AI/Components/  Formation, Crowd-following, Player-agent, Traversal
└── AI/Strategies/  UYieldStrategy + IYieldContextProvider
```

---

🇯🇵 [日本語版](./README.ja.md) | 🇰🇷 [개인 로드맵](./README.ko.md)