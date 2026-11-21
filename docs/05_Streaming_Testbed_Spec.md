# FILE: /docs/05_Streaming_Testbed_Spec.md

**Project:** LOW TIDE | **Phase:** 1 (spec) → built in P2 | **Doc status:** v0.1

Labels: **[T]** = tunable/threshold to confirm in P2. **[VERIFY]** = engine claim (index in 06 §4.6).

---

## 4.1 Streaming Testbed Block Map

**Purpose:** validate 04 (radii, budgets, state machine, memory, degradation) on zero-art graybox content before any P2 gameplay depends on streaming.
**Level:** `L_StreamingTestbed`, in the same project, loading the `LowTideStreaming` gem only. No game gem code is required.

### Grid and area
- The **full 32 × 32 grid at 128 m = 4 096 m × 4 096 m = 16.8 km²**. It deliberately covers the full-vision extents, including the precision-limit corner (2 896 m from origin).
- Content per chunk follows the district density profiles in 04 §3.8, **filled to 100% of budget**. The worst-case chunk is filled to 150%.

### Generation: procedural, two-stage (decision)

| Stage | Tool | Output | Why |
|---|---|---|---|
| 1. Layout + content description | `tools/testbed_gen/generate_testbed.py` (stdlib Python, seeded, deterministic) | Neutral JSON per chunk-layer: boxes, ramps and planes (transform, size, layer, collidable, occluder flag), lights, portals, variant sites, stable IDs | No engine-schema dependency. Diffable. Fast to regenerate |
| 2a. Bulk layers (S, D, H1) | Same script | `.ltchunk` **source** JSON (our own format), consumed by our ChunkData AssetBuilder | Our format, no risk |
| 2b. Prefab layers (G, I, cells) | `tools/testbed_gen/editor_build_prefabs.py`, run inside the Editor via Editor Python bindings (`azlmbr`) [VERIFY EditorPythonBindings API for entity creation and prefab save in 26.05] | `PF_Chunk_X###_Y###_<Layer>.prefab` | Creating prefabs through the editor API stays correct if the `.prefab` JSON schema changes. **Rejected:** writing `.prefab` JSON by hand from Python (undocumented schema, churn) [VERIFY prefab JSON schema stability] |

Primitive assets (authored once by hand or by a Blender script, all graybox):
- `SM_TB_Box_1m` with 4 LODs (LOD0 = a 12-tri cube, padded to class tri counts with bevel subdivisions so LOD budgets are exercised realistically).
- `SM_TB_Ramp_A`, `SM_TB_Plane_10m`, and `SM_TB_Cyl_Tree` (tree proxy, 4 LODs plus a cross-card).
- `MI_TB_Gray`. Debug tints: `MI_TB_Layer_G` (red), `_S` (gray), `_D` (blue), `_I` (green), `_H1` (yellow), `_H2` (orange). Tints can be toggled to show layer/HLOD swaps.

### Zone layout (grid indices, X then Y)

| Zone | Chunks | Profile | Contents | Validates |
|---|---|---|---|---|
| **Z1 Dense urban** | X08–X15, Y16–Y23 (8×8 = 1.05 km²) | Dense urban @100% | 128 m blocks, 15 m streets. Box buildings 10–60 m tall from 8 "kit" box variants. ≥ 8 occluder planes per chunk. 600 G entities + 4 000 bulk per chunk. 24 unshadowed + 2 shadowed lights | Occlusion, entity activation cost, draw budgets |
| **Z2 Industrial** | X16–X23, Y16–Y21 (8×6) | Industrial @100% | 30×60 m sheds, bulk container stacks | Bulk path at scale |
| **Z3 Rural** | X04–X15, Y04–Y13 (12×10) | Rural @100% | Noise-height terrain tiles (±20 m, seeded). 6 000 bulk tree proxies per chunk. Sparse sheds | Terrain LOD rings, vegetation LOD/impostors, H1 at range |
| **Z4 Open water** | X24–X27, Y24–Y27 (4×4) | Water | Water-only chunks + anchored "vessel" proxy (120×25×30 m box stack) with 1 airlock cell | Boat path, open-water H1/H2, airlock |
| **H1 Stress highway** | Row Y08, X00→X31 (4 096 m straight) | Lined both sides with Dense urban @100% chunks (rows Y07, Y09) | 6-lane flat road. At 55 m/s a run takes **74.5 s** | Straight-line throughput at v_max (04 §3.2: 29% I/O, 65% main) |
| **H2 Diagonal highway** | (X02,Y02)→(X29,Y29), 45°, 4.9 km | Dense urban band 3 chunks wide either side | Straight diagonal road | **Worst sustained case** (92% main-thread slice) |
| **H3 Perimeter loop** | Rows/cols 1 and 30, ~15 km | Mixed profiles | Continuous loop for soak tests. Passes all 4 corners (precision) | Long-session stability, memory slope |
| **T1 Corner thrash** | Corner point of X12/X13 × Y20/Y21 (inside Z1) | Dense urban | 4-way intersection exactly on a chunk corner. Figure-8 route, radius 70 m, 20 m/s | Hysteresis at the 4-chunk junction |
| **T2 Edge thrash** | Boundary line Y = 18/19, X16–X23 | Industrial | 1 km road on the chunk edge. Route weaves ±10 m across the line at 30 m/s | Edge hysteresis, min dwell |
| **T3 Radius thrash** | Shuttle path in Z3 at 256 ± 40 m from a dense chunk placed at X10,Y08 | Mixed | Oscillates across the G R_in/R_out band every 4 s | R_in/R_out band, cancellation of in-flight loads |
| **W1 Worst-case chunk** | X14,Y18 (in Z1) | Dense urban @**150%** | 900 G entities, 6 000 bulk, 36 lights, ~375 draws | Degradation ladder (expect L1, never L3). **Budget report must flag it** |
| **I1 Interior stress** | X10,Y22 (in Z1) | Interior | 6 seamless interiors × 400 m² × 400 entities, walkable in sequence in 60 s. 1 vehicle-drive-through garage shell (Mack's workshop analogue). 1 airlock cell 2 000 m² / 1 500 entities | I-layer radius, portal culling, airlock ≤ 1.5 s, G-before-I dependency |
| **P1 Precision corner** | X31,Y31 | Rural | 0.5° ramp with a sphere creeping at ~10 mm/s. Static box stack. Inspect camera at 0.5 m | ULP analysis (04 §3.1): no visible jitter; creep speed error < 10% |
| **F1 Flood ramp** | Z2 | Industrial | Global flood level ramps 0 → 2 m over 60 s while driving through | AD-08: **0 chunk reloads**, depth queries via physics and heightmap |
| **V1 Variant flip** | Z1 | Dense urban | 3 variant sites per chunk. Global toggle flips all in ~25 Active chunks | In-place re-evaluation (rule 5), main-thread cost |
| **VG1 Vignette** | `L_Vig_TB_Room` | — | Box room ↔ world transition at H1 start | Vignette exit with synchronous load ≤ 6 s |

### Instrumentation

**Streaming HUD** (ImGui overlay in profile builds [VERIFY ImGui gem availability in GameLauncher profile]). It reads `StreamingFrameStats` plus engine stats:

| Panel | Fields | Source |
|---|---|---|
| States | Layers by state × layer (10 × 5 matrix), failed count, transitions/frame | ChunkBudgetTracker |
| I/O | Layers in flight, bytes in flight, queue length, measured MB/s (EMA), T_ready EMA per layer | Tracker |
| Main thread | Scheduler / instantiate / deactivate ms (current, avg 1 s, max 10 s) | Tracker |
| Safety | Min G slack (s), nearest unready G distance (m), degradation level, speed cap (m/s), vehicle speed | Tracker |
| Memory | Arena blocks in use / peak, LoadedInactive cache bytes, live entities, live bulk instances, process RAM, VRAM by category | Tracker + O3DE memory stats [VERIFY API] + Atom/RHI memory stats [VERIFY] |
| Render | Draw calls, visible triangles, LOD histogram (bulk path from ILodPolicy counters; entity meshes from Atom stats if exposed [VERIFY]), active occluders | Atom stats [VERIFY] + ours |
| Minimap | 32×32 grid coloured by G-layer state, focus position, velocity vector, pins | Ours |

**Logging:** per-frame CSV, `tests/results/<runId>/raw/frames.csv` (ignored by git):
- frameIndex, frameMs, mainStreamingMs (3 fields), states summary, bytesInFlight, minSlack, degradation, speed, speedCap
- arenaBlocks, cacheBytes, processRamMB, vramMB, drawCalls, visibleTris

Plus a run summary `tests/results/<runId>/summary.json`, which is committed.

### Deterministic benchmark

| Element | Specification |
|---|---|
| Route format | `tests/benchmarks/routes/<Route>.json`: time-parameterised samples (t, x, y, z, yaw, speed) at 10 Hz, Catmull-Rom interpolated |
| Playback modes | **Kinematic** (default, deterministic): streaming focus and camera follow the route exactly, with no vehicle physics. **Physics** (soft-cap tests only): an autopilot vehicle tracks the route, so it isn't bit-deterministic |
| Fixed conditions | 1920×1080, vsync off, fixed lighting state (Day), fixed seeds, profile build, no editor |
| Cache conditions | **Warm** (2nd consecutive run) and **cold** (after purging the OS standby list with Sysinternals RAMMap "Empty Standby List", or after a reboot) |
| Hardware | Dev machine on every run. **Min-spec box at the P2 exit and P3 exit** (see risk R5, 06) |
| Invocation | Launcher command-line/console variable to select a route and auto-quit [VERIFY launcher arg/CVar mechanism]. Exit code = pass/fail |

| Route | Description | Duration |
|---|---|---|
| R1 | H1 straight, 55 m/s, both directions | 2 × 74.5 s |
| R2 | H2 diagonal, 55 m/s | 89 s |
| R3 | H3 perimeter loop, 45 m/s | ~5.5 min |
| R4 | T1 + T2 + T3 thrash sequence | 5 min |
| R5 | I1 interiors on foot, 5 m/s, incl. airlock and garage drive-through | 3 min |
| R6 | Z4 boat route, 25 m/s, ending at the vessel airlock | 2 min |
| R7 | Mixed "MVS-like": urban 15–40 m/s, interiors, F1, V1 | 10 min |
| S1 | Soak: R3 + R5 alternating | 30 min |
| S2 | Soak: S1 repeated | 2 h |

### Pass / fail thresholds [T]

| # | Metric | Threshold | Routes |
|---|---|---|---|
| P1 | Frame time p99 | ≤ 16.6 ms (min spec), ≤ 12.0 ms (dev machine) | All |
| P2 | Hitches | 0 frames > 33.3 ms. ≤ 3 frames > 20 ms per 10 min | All |
| P3 | Main-thread streaming ms | p99 ≤ 2.0 ms, max ≤ 4.0 ms (L1 extension) | All |
| P4 | Gameplay readiness | 0 frames with the focus inside a G chunk that is neither Active nor proxied | All |
| P5 | Degradation | L4 = 0 always. L3 = 0 on R1/R3–R7, ≤ 2 activations on R2. L1 allowed | All |
| P6 | Memory slope | < 1 MB/min (linear regression, after a 5-min warm-up) for RAM and VRAM | S1, S2 |
| P7 | Arena / leak hygiene | 0 LeakReports. Pool free count returns to baseline after a full unload | All |
| P8 | Thrash | ≤ 1 transition per layer per chunk per 3 s window | R4 |
| P9 | Transitions | Airlock ≤ 1.5 s. Vignette exit (synchronous) ≤ 6 s min spec | R5, R6, VG1 |
| P10 | Flood / variants | 0 chunk reloads during the F1 ramp and V1 flip. V1 main-thread cost within P3 | R7 |
| P11 | Precision | P1 corner: no visible vertex jitter at 0.5 m. Creep-speed error < 10% | Manual + scripted check |
| P12 | Budget tool agreement | Runtime draw/entity counts within 15% of `chunk_budget_report.py` | R1, R7 |
| P13 | Stability | 0 crashes, 0 asserts | S2 |

**First measurement in P2 (gates everything above):**
- Entity activation microbenchmark: spawn a G-layer prefab with N = 100, 500, 1 000, 2 000 entities and record µs/entity.
- If > 40 µs, re-derive the chunk radii, entity caps and slice budget in 04 §3.2 **before** building the zones.

---

## 4.2 C++ Interface Contracts

The headers live in `tools/templates/LowTideStreaming/`. They are copied into `LowTide/Gems/LowTideStreaming/` after `create-gem` (03 §2.5).

**Verified:** the engine-independent headers compile with `clang++ -std=c++17` and `-std=c++20` with `-Wall -Wextra -Wpedantic -Werror` (header-only syntax check, macOS).
**Not yet verified:** MSVC/O3DE compile of the two AzCore-dependent headers. That's an Exit Gate item.

| Header | Location | Depends on | Contents |
|---|---|---|---|
| [ChunkTypes.h](../tools/templates/LowTideStreaming/Code/Include/LowTideStreaming/ChunkTypes.h) | Include/ | std | `ChunkCoord`, `ChunkId`, `ChunkLayer`, generational `ChunkHandle`, `StableId`, `PinReason`, `PinId`, `DegradationLevel`, math PODs |
| [ChunkState.h](../tools/templates/LowTideStreaming/Code/Include/LowTideStreaming/ChunkState.h) | Include/ | std | `ChunkState`, exhaustive transition table, invariants I1–I5, `FailureStage` |
| [ChunkStreamingConfig.h](../tools/templates/LowTideStreaming/Code/Include/LowTideStreaming/ChunkStreamingConfig.h) | Include/ | std | Every 04 tunable with defaults and `Validate()` |
| [ChunkMemoryArena.h](../tools/templates/LowTideStreaming/Code/Include/LowTideStreaming/ChunkMemoryArena.h) | Include/ | std | `ChunkBlockPool`, `ChunkArena`, `AllocationTag`, `ArenaCategory` |
| [ChunkBudgetTracker.h](../tools/templates/LowTideStreaming/Code/Include/LowTideStreaming/ChunkBudgetTracker.h) | Include/ | std | Per-layer resource counters, `LeakReport`, `StreamingFrameStats` |
| [ChunkPriority.h](../tools/templates/LowTideStreaming/Code/Include/LowTideStreaming/ChunkPriority.h) | Include/ | std | Pure priority, look-ahead radius, degradation and speed-cap functions |
| [ILodPolicy.h](../tools/templates/LowTideStreaming/Code/Include/LowTideStreaming/ILodPolicy.h) | Include/ | std | `ILodPolicy`, `AssetClass`, `HlodMode` |
| [ICullingPolicy.h](../tools/templates/LowTideStreaming/Code/Include/LowTideStreaming/ICullingPolicy.h) | Include/ | std | `ICullingPolicy` (portals, occluder selection, shadow casters) |
| [IChunkDeltaStore.h](../tools/templates/LowTideStreaming/Code/Include/LowTideStreaming/IChunkDeltaStore.h) | Include/ | std | AD-09 delta store contract |
| [ChunkStreamingBus.h](../tools/templates/LowTideStreaming/Code/Include/LowTideStreaming/ChunkStreamingBus.h) | Include/ | AzCore | `ChunkStreamingRequests` / `RequestBus`, `ChunkStreamingNotifications` / `NotificationBus` |
| [StreamingSystemComponent.h](../tools/templates/LowTideStreaming/Code/Source/Engine/StreamingSystemComponent.h) | Source/Engine/ (private) | AzCore | System component skeleton, pimpl |

ASSUMPTION: The brief's `IChunkStreamingRequests` is named `ChunkStreamingRequests`, following the O3DE bus convention adopted in 03 §2.4.

### Gem CMake skeleton (file list; contents generated by `create-gem`, then edited) [VERIFY template file names]

| File | Purpose |
|---|---|
| `Gems/LowTideStreaming/gem.json` | Gem manifest: name, version, dependencies (none besides AzCore/AzFramework, Atom_Feature_Common, PhysX5) [VERIFY gem dependency names] |
| `Gems/LowTideStreaming/CMakeLists.txt` | Gem root: adds `Code/` |
| `Code/CMakeLists.txt` | Targets: `LowTideStreaming.Core.Static` (std-only Core, no engine deps); `LowTideStreaming.Static` (engine glue, links Core); `LowTideStreaming` (runtime module); `LowTideStreaming.Tests` (AzTest) [VERIFY `ly_add_target`/`ly_create_alias` usage] |
| `Code/lowtidestreaming_core_files.cmake` | Core sources: Source/Core/*.cpp + Include std-only headers |
| `Code/lowtidestreaming_files.cmake` | Engine glue sources: Source/Engine/* |
| `Code/lowtidestreaming_api_files.cmake` | Public headers (Include/LowTideStreaming/*) |
| `Code/lowtidestreaming_shared_files.cmake` | Module entry: `LowTideStreamingModule.cpp` |
| `Code/lowtidestreaming_tests_files.cmake` | Tests/*.cpp |
| `Code/Source/LowTideStreamingModule.cpp` | `AZ::Module` subclass registering `StreamingSystemComponent` descriptor + system component list [VERIFY module macro] |
| `Code/Source/Engine/StreamingSystem.h/.cpp` | Implementation (P2) |
| `Code/Tests/*Tests.cpp` | Unit + integration tests (§4.3) |
| `tests/core_standalone/CMakeLists.txt` | **Engine-free** build of Core + Core tests. GoogleTest pinned via FetchContent (v1.15.2), so the same test sources compile under AzTest and standalone |

---

## 4.3 Test Plan (Phase 2/3 handoff)

**Priority:** pure Core unit tests first. They run standalone in seconds and are written before the engine glue.

### Unit tests (engine-independent, `tests/core_standalone` + AzTest)

| ID | Unit | Test | Pass criteria |
|---|---|---|---|
| U1 | ChunkTypes | Coord↔Id round-trip over the full 32×32 and 64×64 grids; out-of-range rejected | 100% round-trip; invalid → Invalid* |
| U2 | ChunkTypes | `WorldToChunkCoord` at edges: −2 048, 0, +2 047.999 m, and NaN | Correct chunk; NaN/outside → invalid |
| U3 | ChunkHandle | Generation increments per cycle. Stale handle → `Unloaded`. Wrap at 65 535 skips 0 | All asserts hold over 200 k simulated cycles |
| U4 | ChunkState | 10×10 transition matrix vs. the table | Exactly the listed edges valid |
| U5 | State machine (Core scheduler) | Randomised event sequences (seeded, 10⁶ steps): load/cancel/pin/fail/timeout | Never an invalid edge. Pinned never deactivates (I2). I4 dependency respected |
| U6 | Hysteresis | Focus oscillating at R_in ± 32 m and across chunk edges/corners | ≤ 1 transition per layer per dwell window |
| U7 | Priority | Ahead-at-speed vs. closer-behind. Pinned vs. unpinned. Road-path shortcut. Layer factors | Ordering matches 04 §3.3 in all scripted cases |
| U8 | Look-ahead | `ComputeEffectiveRadiusIn` at 0/90/180° and v = 0/55 m/s | +165 m forward at 55 m/s. Never < R_in |
| U9 | Degradation | Slack sweeps across thresholds, incl. step-down hysteresis | Levels per 04 §3.4. No oscillation within 1 s |
| U10 | Speed cap | Monotonic in distance; clamps at 15 m/s; 0 below L3 | Property holds over a sweep |
| U11 | ChunkArena | Alignments 1–64. Exhaustion → nullptr. Oversize → nullptr. Category sums. Release returns blocks | Invariants A1–A5 |
| U12 | ChunkArena (debug) | Write past the end of an allocation | `VerifyIntegrity` false; tag reported |
| U13 | ChunkBlockPool | 4 threads × 250 k acquire/release | No lost/duplicated blocks. Free count returns to N |
| U14 | BudgetTracker | Balanced vs. unbalanced counters per kind | `VerifyReleased` reports exactly the leaked kinds |
| U15 | DeltaStore | Overwrite by id. Cap eviction (cosmetic first, gameplay never). Content hash | Invariants D1–D4 |
| U16 | Save TLV (P3) | Round-trip, unknown section skip, v1→v3 migration chain, CRC mismatch | Bit-exact round-trip. Migration matches the fixture |
| U17 | LodPolicy | Thresholds + hysteresis per class. Shadow LOD ≥ render LOD. HLOD mode exclusivity | L1–L3 |
| U18 | CullingPolicy | Portal truth table. Occluder top-N. Shadow-caster rules | C1–C3 |
| U19 | Variant selection | Global-state snapshot → selected groups | Deterministic. Table-driven fixtures |
| U20 | Zero-alloc tick | Scheduler tick over a 1 024-chunk table under an allocation-counting `operator new` hook | 0 allocations after warm-up |
| U21 | Config | `Validate()` rejects each rule violation | Every rule covered |

### Integration tests (in-engine; AzTest where possible, otherwise a launcher-scripted route)

| ID | Test | Pass criteria |
|---|---|---|
| E1 | Entity activation microbenchmark (§4.1) | µs/entity recorded; ≤ 40 µs, otherwise re-derive radii (blocking) |
| E2 | Spawn/despawn one generated G chunk × 1 000 cycles | 0 LeakReports. RAM delta < 1 MB after cycle 100 |
| E3 | Bulk path: 4 000 instances acquire/release × 100 | Handle counts balanced. VRAM returns to baseline ± 2 MB |
| E4 | Bulk static physics actors | Raycasts hit only while the layer is Active. No hits after Unloaded |
| E5 | Cancellation mid-I/O and mid-deserialize (forced) | Layer reaches Unloaded. 0 leaks. Arena pool baseline restored |
| E6 | Failure injection: corrupt `.ltchunk`, I/O timeout | → Failed. Coarse proxy active. `IsCollisionReady` true. Log emitted |
| E7 | Benchmark routes R1–R7 | §4.1 thresholds P1–P5, P7–P12 |
| E8 | Soak S1 / S2 | P6, P13 |
| E9 | Synchronous load (vignette exit) | ≤ 6 s min spec. Progress reaches 1.0 |
| E10 | F1 flood ramp + V1 variant flip | 0 reloads. Main-thread cost within P3 |
| E11 | Save → unload → reload → load with doors/props changed | Deltas restored exactly (P3: with full save file) |
| E12 | Airlock + vignette transitions | ≤ 1.5 s / ≤ 6 s |
| E13 | P1 precision corner | P11 |
| E14 | ASan: E2, E5 and R1 under the `asan` preset | 0 ASan reports |
