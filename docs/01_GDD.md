# FILE: /docs/01_GDD.md

**Project:** LOW TIDE | **Phase:** 1 (Pre-Production & Technical Design) | **Doc status:** v0.2, reconciled against the Story Bible

## 0. Ground Rules for This Document

- Source references: `Pro` = Prologue · `M1–M17` = main missions · `A1E/A2E` = Act I/II ending scenes · chain names from Bible §8 · `§n` = Bible section.
- ASSUMPTION: Engine = latest stable O3DE release line at project start (25.x) [VERIFY exact tag; pinned in 03]. Engine references here are at capability level only. Exact classes, gems and APIs get confirmed in Parts 2–4.
- Phase key: **P2** graybox prototype · **P3** traffic/AI/mission state machine · **P4** vertical slice art/audio · **P5** full production · **Cut** = not supported by the architecture · **Defer** = hooks kept, not in v1.0.
- ⚑ = architectural flag (expensive to change later). Each flag has a decision in §1.1b.
- Driving is the protagonist's identity (ex-getaway driver and mechanic, §2). Vehicle feel and streaming at speed are first-class requirements, not support systems.

---

## 1.1 Verb List

| # | Verb | Story source | System required | Engine/tech dependency | Stream/Mem | MVS | Phase |
|---|---|---|---|---|---|---|---|
| V01 | Walk / run / sprint | Pro | Character controller, locomotion anim graph | PhysX character controller [VERIFY], EMotionFX [VERIFY] | Low | Y | P2 |
| V02 | Crouch / sneak | M6 stealth, M14 infiltration, M17 | Stance state, noise/visibility emitters | As V01; AI perception (P3) | Low | N | P3 |
| V03 | Vault / climb low obstacles | M14 construction-site pursuit | Traversal probes, anim alignment | EMotionFX root motion [VERIFY] | Low | N | P5 |
| V04 | Take cover | Pro confrontation, M8 standoff, M17 | Cover-point data + runtime validation | Collision queries; per-chunk cover data | Med | Y | P3 |
| V05 | Swim (surface only) ⚑ | Briar swamps §1, Act III flood, M17 (overboard) | Water query, swim state | Global water planes (AD-02); buoyancy forces | Med | N | P4 |
| V06 | Dive underwater ⚑ | — (none in Bible) | Underwater render/post/audio | Second render path | High | N | Cut |
| V07 | Enter / exit interior ⚑ | Pro home, M2 workshop, M1 Saint, M3 archive, M6, M11 station, M17 vessel | Interior cells, portals, transitions | Prefab spawning [VERIFY], occlusion (Part 3) | High | Y (home, workshop) | P2 |
| V08 | Open / lockpick door | M3 closed archive, M6 stealth | Interactable + door state persistence | — | Low (chunk-local) | Y (open) / N (pick) | P2 / P5 |
| V09 | Interact / pick up | Pro (bag, cassette, dropped pistol) | Interaction query, inventory | — | Low | Y | P2 |
| V10 | Enter / exit vehicle | Pro | Seat attach, possession swap | EMotionFX, PhysX | Low | Y | P2 |
| V11 | Drive ground vehicle (≤55 m/s) ⚑ | Pro (drive Lena home), M2 (Mack's car), §2 | Raycast-wheel vehicle model, vehicle camera | PhysX rigid body; no stock vehicle gem assumed [VERIFY] | **High** (sets stream radius) | Y | P2 |
| V12 | Steal vehicle | §9 vehicle-theft missions | Break-in/carjack, ownership flag, witness hook | Anim; NPC reaction (P3) | Low (global registry) | Y (parked break-in) | P2 / P3 |
| V13 | Crash / damage vehicle | M2 escape, M14 pursuit | Health + damage-stage swaps | No soft-body | Med | Y (health only) | P2 / P5 |
| V14 | Store / retrieve vehicle | Mack's workshop §3 | Player-vehicle registry | Save system | Low | N | P5 |
| V15 | Customize vehicle | Mack's workshop §3, §9 | Paint params, perf tiers | Material params [VERIFY Atom material property API] | Med | N | P5 |
| V16 | Race | M9, Tam chain, §9 street racing | Checkpoints, rules, AI racers | Spline AI (P3) | **High** | N | P5 |
| V17 | Evade police / heat | M2 escape (ASSUMPTION), M9 raid, M14, §10 police attention | Heat tiers, dispatch | Traffic/AI (P3) | **High** ⚑ (AD-10) | Y (tier 1) | P3 |
| V18 | Pilot boat ⚑ | §9 boating, M17 approach to anchored vessel | Buoyancy-sample boat physics | Global water plane, PhysX | **High** (open-water sight-lines) | N | P5 |
| V19 | Board vessel from boat | M17 | Boat→vessel transition | Interior cell | Med | N | P5 |
| V20 | Fly aircraft ⚑ | Briar airfields §1 (location only) | — | Vertical streaming, whole-map HLOD | High | N | Cut |
| V21 | Melee brawl | Pro confrontation | Hit volumes, reactions, stamina | EMotionFX anim events [VERIFY] | Low | Y | P2 / P3 |
| V22 | Box (gym bouts) | Old Mercy gyms §1, §9 | Melee subset + bout rules | As V21 | Low | N | P5 |
| V23 | Aim / shoot / reload | Pro escalation (ASSUMPTION), M6 direct, M17 escape | Weapon system, hitscan, ammo | Scene raycasts [VERIFY], decal/VFX pools | Med | Y (1 pistol) | P2 / P3 |
| V24 | Switch / holster weapon | Pro | Inventory slots, anim layers | EMotionFX | Low | Y | P2 |
| V25 | Stealth takedown | M6, M14, M17 | Paired anims, awareness | AI perception (P3) | Low | N | P5 |
| V26 | Break line of sight / hide | M6, M14 | LOS + light-level visibility | Light sampling method [VERIFY] | Low | N | P3 |
| V27 | Throw distraction | — | — | — | Low | N | Cut |
| V28 | Tail / follow lead | M8 missing union rep, Sofia chain | Distance/LOS mission condition | AI schedules (P3) | Med (target pins chunks) | N | P3 |
| V29 | Examine physical evidence | Pro (eviction notice, cassette), M2, M3 documents/photos | Inspect camera, rotate, clue tags | Force-resident hero mips [VERIFY] | Low | Y | P2 / P4 |
| V30 | Listen to audio evidence | Pro cassette, M11 recording, Lena chain | Inspect-mode audio playback | Audio streaming | Low | Y | P2 |
| V31 | Review footage / compare statements | M3 | Evidence viewer | Video playback [VERIFY]; see AD-14 | Med | N | P5 |
| V32 | Collect Atlas piece | §5 (scattered records/recordings) | Atlas registry (global bitset) | Save | Low | Y (cassette = #1) | P2 |
| V33 | Link evidence on case board | M3, A1E "The Map" | Deduction UI | UI gem [VERIFY LyShine vs. current UI stack] | Low | N | P5 |
| V34 | Photograph | §9 | Phone camera, RTT | Atom RTT [VERIFY] | Med | N | Defer |
| V35 | Dialogue choice | All missions | Dialogue graph runtime, variable writes | UI, VO streaming | Low | Y | P3 |
| V36 | Negotiate / intimidate / persuade | M1, M4, M12, M15 | Dialogue conditions on traits/trust | — | Low | N | P5 |
| V37 | Social infiltration | M6, M17 | Credential/dialogue access gating (no disguise system) | — | Low | N | P5 |
| V38 | Choose approach / plan op / recruit crew | M6, M17, Mack crew §3 | Approach selection, crew roster | Mission SM parallel branches (P3) | Med (per-approach spawn sets) | N | P5 |
| V39 | Converse while driving | Pro (Lena drive) | In-vehicle dialogue, interruption handling | VO streaming vs. chunk I/O priority | Med | Y | P3 |
| V40 | Use phone (calls/texts) | Pro cheap phone, M3 Rafe contact | Message queue, phone UI | UI | Low | N | P5 (mission comms) |
| V41 | Act with personality consequence | §2 personality system | Trait ledger (6 traits) | Global state | Low | Y | P3 |
| V42 | Shop / buy / sell | §9 | Shops, wallet | UI | Low | N | P5 |
| V43 | Own property / manage business | §9, M1 Saint hub outcome | Ownership table, state-driven variants | Chunk instantiation reads global state | Med | N | P5 |
| V44 | Gamble | §9 poker rooms/casinos | Minigame, seeded RNG | UI | Low | N | P5 |
| V45 | Fish | §9 | Minigame, water query | Water plane | Low | N | Defer |
| V46 | Hunt | Briar hunting properties §1, §9 | Wildlife AI, ecology | Animal AI + animation | Med | N | Cut |
| V47 | Evacuate (drive residents, set priorities) | M13 | NPC passenger boarding, shelter drop-off, priority choice | Crowd AI (P3), global event state | **High** | N | P5 |
| V48 | Wade / drive through flood | M14, Act III | Depth = floodLevel − groundHeight; drag/stall | Global water param ⚑ | **High** | N | P5 |
| V49 | Survive storm | Act III | Weather state, hazard triggers | Weather params | Med | N | P5 |
| V50 | Defend location | M11 protect station | Defend objective, wave spawns | AI (P3) | Med | N | P5 |
| V51 | Save / rest at safehouse | Pro family home | Save system, TOD advance | Serialization (Part 3 §3.10) | Low | Y | P2 / P3 |
| V52 | Change outfit | §9 shopping (inferred) | Character part swaps | Mesh/material swap | Med | N | Defer |
| V53 | Heal / recover | Inferred | Health regen | — | Low | Y | P2 |
| V54 | Set waypoint / GPS | Inferred | Map UI, road-graph routing | Global road graph | Low | Y | P2 / P3 |
| V55 | Enter nightlife venue | M1 The Saint, M7 Velvet Room, §9 | Venue hooks (§1.4) | Interior cell | Med | N | P5 (Saint) / Defer (Velvet) |
| V56 | Romance / intimacy scenes | §9 | Relationship flags, scene gating | Content gating | Med | N | Defer (§1.4) |
| V57 | Watch scripted sequence | Pro, M16 flashback, A1E, A2E, endings | Camera tracks, sequence player | Track View or equivalent [VERIFY] | Med (preload) | Y | P3 / P4 |
| V58 | Ride as passenger | Pro bus | Passenger seat, spline-driven vehicle | Spline component [VERIFY] | Med (deterministic path) | Y | P3 |
| V59 | Ride motorcycle | Ash Dogs §4, Tam chain | Arcade-stabilized bike model | PhysX | High (same speed class) | N | P5 |
| V60 | Play flashback | M16 | Past-variant location, control swap | Vignette cell (AD-12) | Med | N | P5 |
| V61 | Destroy environment ⚑ | §9 storm-damage events | Prop breakables only | PhysX rigid bodies, pooled debris | Med | N | P5 props / structural Cut |
| V62 | Decide Atlas fate (ending) | M17, §7 | Ending resolver + gates | Global state | Low | N | P5 |
| V63 | Build trust (character arcs) | §8, §10 | Per-character trust + outcome enums | Global state | Low | Y (Lena, Mack writes) | P3 |
| V64 | Listen to radio (Lena's station) | Old Mercy station §1, Ending A | In-car radio stream | Audio streaming | Low | N | Defer |

### 1.1b Architectural Flags: Decisions

| Flag | Decision (final) | Trade-off | Rejected alternatives |
|---|---|---|---|
| AD-01 Interiors | **Hybrid.** **Seamless interior cells** (≤ ~400 m² footprint, tunable) are child content of the exterior chunk, behind door portals: family home, Mack's workshop (vehicle-sized portal; cars drive in and out), radio station, bars, gyms, shops. **Airlock interior cells** are streamed separately behind a ≤1.5 s door anim/fade that covers a guaranteed-complete preload: The Saint, municipal archive, casino back rooms (M6), Meridian facility, vessel decks, Rafe's penthouse, Velvet Room (deferred). | Two code paths. In exchange, peak memory is bounded. | All loading screens: breaks feel, still needs streaming. All seamless: memory spikes and portal authoring at hero scale. |
| AD-02 Water | **Global water planes only:** sea level (sea, marina, swamps, canals) plus **one global flood level**. No FFT ocean, no fluid sim. Depth is analytic. | Flat water, no gameplay-relevant waves. | Gem/third-party ocean: dependency risk [VERIFY availability]. Fluid sim: beyond capacity. |
| AD-03 Swimming | **Surface swim only.** Diving is cut; the Bible has no underwater content. | — | No swimming: unacceptable with swamps, flood and overboard falls. |
| AD-04 Flight | **Permanently cut.** Briar airfields become race and event venues (long runways suit M9 top-speed racing). Police heat has no helicopters. | Lose a genre staple. | Flight: vertical streaming plus whole-map HLOD. |
| AD-05 Destruction | **Props only** (pre-authored intact→broken swaps, pooled debris, lifetime cap). Storm damage (§9) = pre-authored variant groups (AD-09). | Static architecture. | Runtime fracture: CPU, memory and authoring cost. |
| AD-06 Vehicle top speed | **Hard cap 55 m/s.** The vehicle system exposes a **speed soft-cap hook** the streamer can drive (Part 3 §3.4). | Slight artificial ceiling. | Uncapped: makes the stream radius unbounded. |
| AD-07 Time of day | **Discrete lighting states: Day / Twilight / Night / Storm**, blended at mission boundaries or fades. The prologue exercises Twilight→Day→Night. Only one state's probes are resident. | Less continuous "living world". 4 lighting variants max. | Continuous TOD: real-time GI cost. Fixed time: the story needs dawn, night and storm. |
| AD-08 Flooding | **Global flood-level variable** drives the water plane height, a material wetness/height parameter, and depth queries. **No per-chunk variant swaps for water.** | Flood extent follows terrain height only. Fine for the narrative point (§5: low-lying areas flood). | Chunk variant swaps: doubles data, forces reloads. Simulation: out of scope. |
| AD-09 Persistence + visible consequences | **(a) Chunk-local delta records** (doors, broken props, looted containers, moved parked vehicles), keyed by stable authored IDs. **(b) State-driven variant groups**: authored entity groups toggled at chunk instantiation from global state (§10: businesses opening/closing, abandoned properties, Act III closures, barricades). Hard cap: **≤ 2 states per variant site** and **≤ 12 variant sites per district** (tunable). Chunks read global state and never own it. | Visible reactivity is bounded and authored, not systemic. | Full chunk snapshots: save and load cost. Systemic dressing: authoring and QA explosion. |
| AD-10 Off-bubble simulation | **No entity simulation outside Active chunks.** "Events happen without Tyler present" (§9) is implemented as **abstract resolution**: the global event scheduler resolves outcomes and writes global state, which appears through AD-09 variants when the player arrives. Pursuers and tail targets are bound to the player bubble. Mission-critical NPCs pin chunks. | No real off-screen agents; the world still changes. | Full off-screen sim: CPU cost plus streaming chunks nobody sees. |
| AD-11 Open water | Water areas are chunks like any other. The anchored casino vessel sits in an open-water chunk with coastline/skyline HLOD. **Boat speed cap 25 m/s** (tunable). | Slower boats. | 55 m/s on water: sight-lines and wake VFX blow the far-LOD budget. |
| AD-12 Vignette cells | Off-grid levels outside the open-world grid, entered through airlock transitions: **prison gate (Pro)** and **Last Ferry flashback (M16)**. They have their own small memory budget and are never resident at the same time as the full open-world bubble. | Transition seams. | Building the prison and past-ferry inside the world grid: permanent memory and authoring cost for one-time scenes. |
| AD-13 Non-playable skyline | Deferred or cut districts (Glass Mile core, Northbank, Pelican Keys silhouette) exist only as **far-tier HLOD backdrop meshes**: no collision, no near LOD, always-resident far tier. Needed for the prologue skyline reveal and the "beautiful from a distance" brief (§1). | The skyline can't be visited. | Full districts: the cost is the reason for the cut. Skybox matte: breaks parallax from moving vehicles. |
| AD-14 Video evidence | **No video file playback.** Security footage (M3) = in-engine replay of an authored sequence, or a still-frame sequence in inspect mode. | Less authentic "CCTV" look. | Video decode pipeline [VERIFY Atom/gem support]: extra dependency and memory. |

---

## 1.2 Systems Requirement Matrix

| System | Data persisted | Update frequency | Scope | Est. save size* | Streaming/save implication |
|---|---|---|---|---|---|
| Personality traits (§2) | Loyalty, Ruthlessness, Empathy, Ambition, Reputation, Trust: `int16` each | Event-driven | Global | 12 B | Dialogue/ending conditions only |
| Character trust + outcomes (§3, §8, §10) | Trust `int16` + outcome enum + flags[32] for Lena, Inez, Mack, Tam, Dime, Sofia. Rafe stance enum (ally/rival/enemy) | Event-driven | Global | ~16 B × 7 | Outcome enums select epilogue variants and hub availability |
| Faction influence (§4, §10) | `int16` per (faction × district): Calder, Ash Dogs, Lanterns, Meridian | Event-driven + 1/min game-time tick | Global | 4×6×2 B | Read at chunk instantiation (spawn tables, AD-09 variants) |
| Civilian trust (§10) | `int16` per district | Event-driven | Global | 12 B | Drives the district condition variants (AD-09) |
| Police attention (§10) | Global scalar + per-district patrol density | Event-driven; tick 1/min | Global | <32 B | Patrol spawn density per chunk (P3) |
| Heat / wanted | Tier, last-seen position | Per frame while active | Global, transient | 0 (cleared on safehouse save) | Pursuers bound to the player bubble |
| NPC memory: named/notable | met, attitude, last-interaction id, flags[32] | Event-driven | Global | ~64 B × ~120 (cast + notable locals + venue staff) ≈ 8 KB | Spawned through the global registry, not chunk prefabs |
| NPC memory: ambient | Not persisted. Witness heat = per-chunk decaying scalar | Perception tick (P3) | Chunk-local, transient | 0 | Discarded on unload |
| Dynamic world events (§9) | Per event type: state, seed, cooldown, last outcome | 1 Hz scheduler | Global schedule; instance = chunk-local while Active | ~16 B × types | Off-bubble = abstract resolution writing global state (AD-10) |
| Business / venue status | The Saint (independent / player-owned / Calder), Velvet Room (reserved), other businesses open/closed | Event-driven | Global | <256 B | AD-09 variant selection; hub availability |
| World phase + Act III crisis | Act (I/II/III), crisis substate, road-closure set (bitset over road-graph edges), schedule set id | Event-driven | Global, always resident | <1 KB | Road graph edges disabled; barricade/closure variants (AD-09); no chunk reloads |
| Weather / flood | Weather state, intensity, **flood level** | Render params per frame (interp); state event-driven | Global, always resident | <64 B | Every chunk's water/depth queries read the flood level (AD-08) |
| Time-of-day state | Lighting state enum | Event-driven | Global | <16 B | Swaps resident probe set |
| Atlas evidence (§5) | Collected bitset, examined bitset, **shared-with (Lena/Sofia/none) per piece**, case-board links | Event-driven | Global | ≤64 pieces → ~0.5 KB | Chunk spawns a piece only if uncollected. No chunk save |
| Branching missions | Per mission: status, chosen approach, branch id, checkpoint id, ≤16 typed vars | Event-driven; active mission per frame | Global | ~64 B × ~40 | Active mission pins chunks. Mission entities are mission-owned, not chunk-owned |
| Evacuation outcomes (M13) | Per-community assisted flags + priority order | Event-driven (M13 only) | Global | <64 B | Feeds M17 support availability |
| Endings (§7) | Derived from traits, trust, outcomes, Atlas count and the M17 Atlas decision. Store the decision only | Event-driven | Global | <16 B | Gate D: Sofia trust ≥ T and Atlas ≥ N (tunable) |
| Property ownership | Safehouses, businesses, income timers | Event-driven + 1/min tick | Global | <1 KB | AD-09 variants |
| World-object state | Delta records {stable id, state, optional transform} | Event-driven | **Chunk-local**, persists across unload | ≤256 records × ~32 B per chunk (cap, tunable) | In-RAM delta store on unload, applied on reload, written in the save |
| Player vehicles | Archetype, paint, perf tier, damage, position | Event-driven | Global | ~64 B × ≤16 | Respawned through the registry when the host chunk becomes Active |
| Economy | Cash, owned items | Event-driven | Global | <1 KB | — |
| Player | Transform, health, inventory, stance | On save | Global | <4 KB | Save position must resolve to a chunk loadable synchronously at boot |

\*Starting estimates. Validated by measuring real save size in P3. Total save estimate is well under 1 MB, plus the chunk delta store.

**Rules derived from this matrix (binding for Part 3):**
1. Global state is always resident and never streamed.
2. Chunks *read* global state at instantiation (spawn tables, variant groups, collectibles) and *emit events* to it. They never own authoritative global data.
3. Chunk-local persistence = delta records only, against immutable authored chunk content.
4. Mission-, registry- and event-spawned entities are not chunk-owned. They reference chunks by ID (pinning), never by pointer.
5. A global-state change during play that affects already-Active chunks (Act III phase flip, business closure) is applied by **re-evaluating variant groups in place**. It never triggers a chunk reload.

---

## 1.3 Story-to-Engineering Translation: Set-Piece Risks

| Set piece | Technical risks | Mitigation direction (phase) |
|---|---|---|
| **Prison-bus arrival** (Pro) | (1) Vignette→open-world transition (AD-12) with an empty cache. (2) Scripted bus path crosses many chunks. (3) **Skyline reveal** needs far HLOD of districts that aren't playable (AD-13). (4) Passenger camera framing vs. roadside LOD pop. (5) Twilight→Day→Night lighting transitions in one session. (6) In-car VO (Lena drive) competes with chunk I/O. | Deterministic path → **precomputed preload list**. Bus speed slaved to stream readiness. I/O priority classes: collision > VO > textures (Part 3 §3.4). Lighting swaps only at fades. |
| **Evacuation day** (M13) | (1) Crowds and traffic above the normal per-chunk entity budget. (2) NPC passengers boarding the player vehicle. (3) District-wide state flip (closures, barricades, emergency vehicles). (4) Gridlocked vehicles can't be despawned. (5) AI CPU spike under storm rendering. | "Event density" budget tier. Variant groups re-evaluated in place (rule 5). Crowd LOD/impostors are a P3 dependency. Priority-choice UI is decoupled from simulation. |
| **Act III flooded streets** (M14) | (1) Vehicle pursuit at speed through flood water: splash VFX, drag, stall. (2) Transparent water overdraw plus reflections at 1080p. (3) Rain particles, wet materials, lightning, and the night budget all at once. (4) Debris counts. (5) Flood level saved and restored. | AD-08. Storm fog **shortens draw distance**, so drop HLOD tiers in Storm state. Lightning = one shadowless directional flash. Pooled debris with a cap. Splash VFX pooled per vehicle. |
| **Casino vessel finale** (M17) | (1) **Anchored offshore**, so a boat approach (V18) is mandatory. (2) Multi-deck interior plus open decks with a night-city skyline, in Act III storm: night + storm + water + interior is the worst-case render state. (3) Multiple entry approaches = multiple spawn/AI sets in one cell. (4) High unique-material density. | **Vessel static** (no moving-platform physics); motion is implied by water/sky only. Decks = exterior content of an open-water chunk; hull = airlock interior cells per deck. Approach spawn sets load per chosen approach only. Material budget exception logged in Part 3 §3.8. |
| **Briar County racing** (M9) | (1) Sustained 55 m/s; airfield runways allow peak speed. (2) Night underground event: **N vehicles × headlights** as dynamic lights. (3) Crowd at the event plus a police raid → racers, police and crowd all at once. (4) Tunneling (CCD). (5) Rural sight-lines → terrain and vegetation impostors. | Closed courses → precomputed preload corridor. **Only the player's headlights cast shadows**; AI headlights are unshadowed with a capped count. CCD on vehicles [VERIFY PhysX CCD config]. Raid escape route is pre-authored. |
| **Last Ferry flashback** (M16) | (1) Past-version of a live location: risk of duplicating the chunk. (2) Long scripted sequence plus control swap. | Vignette cell (AD-12) built from the same kit as the present-day pier. Present-day pier stays in the world grid. |
| **Cash Never Sleeps heist** (M6) | Multiple approaches × one interior = combinatorial AI/spawn/nav setups. | Ship 2 approaches (02). Approach = data set loaded into the airlock cell. |
| **Night city** (Glass Mile edge, Old Mercy) | Neon/emissive density, dynamic light count, shadow-caster limits. | Per-chunk light budget with a shadow-caster cap (Part 3 §3.8). Confirm the Atom light culling model and limits [VERIFY]. |

---

## 1.4 Content-Sensitivity Note (Adult Venues, Intimacy)

**Status: NON-MVS.** Velvet Room (M7) and intimacy scenes (§9) are **deferred** (02 §2.2). The Saint (M1) is kept as a nightclub with no adult content. No content design here. These hooks are reserved:

| Hook | Reserved as |
|---|---|
| Venue as a faction-economy node | Property/faction table entry: owner faction, status enum (§1.2 Business status), income, influence modifier |
| Workers as characters (§4) | Staff = named/notable NPC memory records with flags, same as any cast member |
| NPC schedule | Venue exposes schedule slots (hours, staff/patron archetype ids) to the P3 schedule system |
| Interior streaming | Venue = AD-01 airlock interior cell. Shares the interior memory budget |
| Relationship/intimacy gating | Relationship flags in character records. Scenes are referenced by id, never baked into level data |
| Content gating | Global `contentFlags` bitmask read at cell/scene instantiation, so content can be removed or swapped per rating/region as a data change |

Rating implications: crime violence plus nudity/sexual content targets ESRB **M**/PEGI 18. **Explicit** interactive sexual content risks **AO**, which limits storefront and marketing reach. Steam requires the Mature Content survey and may gate visibility. Storefront and payment-processor policies on adult content changed during 2025 [VERIFY current Steam policy].

---

## Scope Flags (out-of-scope blocking dependencies; not addressed further)

| Flag | Why it blocks | Owner phase |
|---|---|---|
| Mission scripting runtime (C++ data-driven vs. Script Canvas/Lua) | Decides mission-state serialization (§1.2). Recommendation for P3: C++ data-driven graphs, so no script VM state needs saving | P3 |
| Parallel-approach missions (M1, M6, M12, M17) | The mission SM must support approach branches with per-approach spawn sets (V38) | P3 |
| Vehicle physics source (custom raycast vs. any O3DE vehicle gem) | V11 is MVS-critical [VERIFY gem existence/maturity] | P2 week 1 |
| Water rendering source (custom plane material vs. Gem) | AD-02 | P4 |
| Adult-content policy for v1.0 | Owner decision (rating target). Engineering only needs `contentFlags` | Before P5 |
