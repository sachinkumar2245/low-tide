# FILE: /docs/04_World_Streaming_Design.md

**Project:** LOW TIDE | **Phase:** 1 | **Doc status:** v0.2: engine claims checked against the O3DE **26.10.0** source (tag `2610.0`, commit `b6122815`) | **Engine:** O3DE 26.10.0 (03 §2.1)

Labels:
- **[T]** = tunable starting value, validated by the P2 testbed (05) and P3 profiling.
- **✔ V-nn** = confirmed in the 2610.0 source (evidence in 06 §4.6).
- **[VERIFY]** = still open (runtime measurement or not settleable from source).
- ASSUMPTION lines are unmeasured inputs.

**v0.2 changes driven by source verification:**
1. Atom has **no render thread** and PhysX simulation **blocks the main thread**, so the main-thread budget was rebuilt (§3.0).
2. Prefab spawns are **not time-sliced**: we slice them with batched `SpawnEntities(indices)`. Queued spawns **can't be cancelled** (§3.3).
3. Entity-less meshes are viable, but **per-handle cost** rules out 256 k handles. Static kit geometry is **merged offline into per-chunk sections**, and props use instanced handles (§3.4).
4. Atom LOD is **per mesh** (coverage + decay), with no cross-fade and no shadow LOD (§3.7).
5. Packed `_ORM` textures are unsupported by StandardPBR, so maps are separate (§3.8).
6. Probes bake to **R9G9B9E5**, so the probe memory was recomputed (§3.5).

---

## 3.0 Targets and Frame Budgets

### Minimum spec (defined here)

| Component | Min spec | Justification |
|---|---|---|
| CPU | 6C/12T, 2019-class (Ryzen 5 3600 / Core i5-10400) | The main thread carries game, RPI, physics sync and spawn processing; ≥ 5 job workers for Atom recording/culling, the PhysX solver and asset loads |
| RAM | 16 GB | 6 GB process ceiling plus OS and background |
| GPU | 6 GB VRAM, GTX 1660 Super / RX 5600 XT class. Atom requires `D3D_FEATURE_LEVEL_12_0` ✔ V-48. **No ray-tracing hardware assumed**: real-time DiffuseProbeGrid is unavailable (✔ V-61) | 4 GB budget + ~2 GB OS/driver margin |
| Storage | **SATA SSD (≥ 450 MB/s seq). HDD unsupported** | §3.2 assumes ≥ 150 MB/s effective |
| OS | Windows 10 22H2 / Windows 11 | — |

### Frame budget at 60 FPS (16.6 ms), min spec, 1080p [T]

**Engine facts:**
- Atom runs RPI simulate/culling/render tick **on the main thread** and fans work out to jobs. Command lists are recorded on jobs, and each command queue has its own submit thread (✔ V-49).
- PhysX `Simulate` runs at `TICK_PHYSICS_SYSTEM` (200) and **blocks the main thread** in `FinishSimulation` (✔ V-44).
- Spawn requests are processed on the main thread at `TICK_GAME` (80) (✔ V-37).

| Unit | Budget (ms) | Contents |
|---|---|---|
| **Main thread** | **16.6** | Input + game + AI (P3) 3.0 · **streaming 2.0** (scheduler 0.2 + instantiation 1.4: spawn-batch processing at TICK_GAME, mesh-handle acquire, physics batch add · deactivation 0.4) · physics simulate wait 2.5 · animation sync 1.0 · **Atom RPI simulate/culling/render tick 4.0** · asset/system tick (incl. `AssetManager::DispatchEvents`) 0.5 · UI 0.5 · audio 0.3 · engine/EBus 1.0 · **slack 1.8** |
| **Job workers** | Background | Atom command-list recording + culling, PhysX solver, AssetManager load jobs (legacy JobManager), our post-load jobs (TaskGraph) |
| **GPU** | 16.6 | Depth pre/opaque 6.0 · shadows 3.0 · transparency/water 1.5 · post 2.0 · other 2.0 · slack 2.1 |

The main thread is the binding constraint (risk R4, 06). Slack is only 1.8 ms, which is why streaming work is strictly sliced.

---

## 3.1 World Coordinate System

### Precision analysis

float32 has a 24-bit significand. For |x| in [2^e, 2^(e+1)), ULP = 2^(e−23).

| Distance from origin | ULP | Smallest motion per 60 Hz step | Pixel size at 0.5 m camera distance (1080p, 60° vFOV) |
|---|---|---|---|
| 1 024–2 048 m | 0.122 mm | 7.3 mm/s | 0.53 mm |
| **2 048–4 096 m** | **0.244 mm** | **14.6 mm/s** | 0.53 mm |
| 4 096–8 192 m | 0.488 mm | 29.3 mm/s | 0.53 mm |
| 8 192–16 384 m | 0.977 mm | 58.6 mm/s | 0.53 mm |

**Safety criterion:** ULP ≤ 0.25 mm. Two reasons:
1. Vertex quantization stays under half a pixel in the inspect-mode close-up.
2. The slowest resolvable motion (14.6 mm/s) is well below the sleep region. O3DE's default sleep threshold is `m_sleepMinEnergy = 0.005` (✔ V-69); as kinetic energy per unit mass ½v², that equals ≈ 0.1 m/s. Quantization only affects bodies that are about to sleep anyway.

**→ Maximum safe distance from origin = 4 096 m.**

### World extents check
- Grid = **32 × 32 chunks × 128 m = 4 096 m square**, origin at the grid centre. The corner is at **2 896 m < 4 096 m** ✓.
- The Atom visibility octree root spans ±16 384 m (`bg_octreeMaxWorldExtents`) (✔ V-53), so the world sits well inside it.
- The grid can grow to 5.79 km square before violating the criterion.

### Decision: single fixed origin, float32 world positions, chunk-local authored coordinates

| Aspect | Rule |
|---|---|
| Runtime positions | float32 world space |
| Origin | Fixed at the grid centre. **No origin shifting** |
| Authored data | Chunk content stored relative to the chunk origin (an exact integer × 128 m) |
| Guardrail | Debug/profile assert if any entity's \|x\| or \|y\| exceeds 3 000 m |
| Time | Game clock is a double. Shader time wraps every 3 600 s |

Engine facts that make this the only sane choice:
- `AZ::Vector3` is float-only, and **no double-precision/large-world option exists** (✔ V-71).
- PhysX origin shifting is **not exposed** (✔ V-70).
- Atom uploads float `float3x4` object-to-world and float view positions, with **no camera-relative rendering** (✔ V-50).

*Rejected:* a hand-built floating origin (rebase every transform and re-add every body: no engine support, unnecessary under 4 km); doubles (not available).

---

## 3.2 Spatial Partitioning

### Decision: uniform 128 m grid + two-tier HLOD (hierarchical grid)

| Option | Verdict |
|---|---|
| **Uniform 128 m grid, per-layer radii, HLOD T1 (per chunk) and T2 (4×4 clusters)** | **Selected.** O(1) mapping, trivial naming/generation, predictable load units |
| Quadtree | Rejected: variable load units make slicing and budgeting unpredictable |
| Single-level grid without HLOD | Rejected: no skyline or long sight-lines |

### Chunk size derivation (128 m)

| Input | Value | Source |
|---|---|---|
| v_max | 55 m/s | AD-06 |
| Effective read throughput, min spec | 150 MB/s [T] | 33% of SATA sequential |
| G-layer payload (worst urban) | 6 MB [T] | §3.8 |
| S-layer payload (worst) | 8 MB [T] | Merged section meshes + probes (§3.4) |
| Entity spawn+activate cost | 25 µs/entity [T] | **Unmeasured. E1 is the first P2 measurement** |
| Mesh-handle acquire cost | 20 µs/handle [T] | **Unmeasured.** Each handle creates a ModelDataInstance, MeshLoader, TransformService id, ObjectSrg and octree entry (✔ V-51) |
| Entities per urban G layer | ≤ 600 | §3.8 |
| Mesh handles per urban S layer | ≤ 200 merged sections | §3.8 |
| Main-thread instantiation slice | 1.4 ms/frame | §3.0 |

**G-layer ready time (worst):**
- I/O: 6 MB / 150 MB/s = **40 ms**
- Asset parse (AssetManager load job) + our post-load job: **≈ 30 ms** [T]
- Spawn: 600 × 25 µs = 15 ms of main-thread work at 1.2 ms/frame of spawn batches = 12.5 frames = **208 ms**
- **T_chunk ≈ 0.28 s**

**S-layer instantiation:** 200 handles × 20 µs = **4 ms** (≈ 3 frames at the remaining slice).

| Chunk size | Resident S chunks (worst) | Per-chunk spawn (urban G) | Verdict |
|---|---|---|---|
| 64 m | ~169 | ~4 ms | Per-chunk overhead (tickets, asset requests, handles, seams) dominates |
| **128 m** | **~64 (≈ 43 euclidean)** | **~15 ms (≈ 12.5 frames)** | **Matches the city-block authoring unit; bounded spikes** |
| 256 m | ~16 | ~60 ms (50 frames) | +31% resident area; coarse cancellation; editor prefab scale |

### Stream-in radius vs. speed vs. load time (straight line at v_max, dense urban)

| Quantity | G layer (R_in 256 m) | S layer (R_in 384 m) |
|---|---|---|
| New chunks per 128 m travelled | 6 | 8 |
| Time per 128 m at 55 m/s | 2.33 s | 2.33 s |
| Demand | 2.6 chunks/s | 3.4 chunks/s |
| I/O | 15.6 MB/s | 27 MB/s |
| Main-thread work | 2.6 × 15 ms = 39 ms/s | 3.4 × 4 ms = 13.6 ms/s |
| **Combined** | **I/O 43 MB/s = 29% of 150 MB/s.** Main 53 ms/s = **0.88 ms/frame = 63% of the 1.4 ms slice** ✓ | |
| Diagonal (×1.41) | I/O 41%, main **1.24 ms/frame = 89%** ⚠ (worst sustained case) | |

**Lead-time check (G layer):**
- Time to contact from R_in = 256 m / 55 m/s = **4.65 s**.
- Worst ready time = queue (6 ahead × 0.208 s) + 0.28 s = **1.53 s**.
- **Safety factor 3.0.**
- Minimum admissible R_in = 55 × 1.53 × 2 = **168 m**, so 256 m holds.
- Velocity look-ahead adds +165 m forward.

**Synchronous loads behind a fade:**
- I/O: 36 × 6 MB + 64 × 8 MB = 728 MB → 4.9 s.
- Unsliced spawn + handles: 36 × 15 ms + 64 × 4 ms ≈ 0.8 s.
- **≈ 6 s** (MVS D9 ✓).

### Grid, coordinates and density profiles

| Item | Value |
|---|---|
| Grid | 32 × 32 = 1 024 cells. `Chunk_X000_Y000` = SW. Chunk (x,y) min corner = ((x−16)·128, (y−16)·128) |
| Shippable occupancy | ≈ 300–350 non-empty chunks |
| Prefab/level constraint | `L_World` holds **only global entities**. Chunk layers are separate spawnables. Authoring uses district context levels. Prefab entity ids are hash-stable while the entity alias is unchanged (✔ V-28). Per-layer prefab cap 1 000 entities [T] (editor responsiveness, [VERIFY] empirically, R9) |

| District profile | G prefab entities | S merged-section handles | D instanced prop handles |
|---|---|---|---|
| Dense urban (OM, GM edge) | ≤ 600 | ≤ 200 | ≤ 250 |
| Industrial (TC) | ≤ 400 | ≤ 150 | ≤ 200 |
| Rural (BC) | ≤ 200 | ≤ 120 (terrain + merged veg clusters) | ≤ 400 (trees as instanced handles) |
| Water / offshore | ≤ 50 | ≤ 20 | ≤ 20 |

**Global handle cap:** ≤ **24 000 resident mesh handles** [T].
- Derivation: 64 S × 200 + 16 D × 250 + ~36 G × 100 entity meshes + ~300 dynamic ≈ 20.7 k, plus margin.
- Atom's `Simulate` walks every handle each frame in jobs (✔ V-51): "thousands fine, hundreds of thousands not". The P2 benchmark measures Simulate ms per 10 k handles (E3).

### Chunk content taxonomy and residency

| Content | Residency | Layer / radius (R_in / R_out) [T] | Notes |
|---|---|---|---|
| Terrain render tile | Per chunk | **S** 384 / 448 m | Mesh tiles. Ring LOD set via `LodType::SpecificLod` (§3.7) |
| Terrain + static collision | Per chunk | **G** 256 / 320 m | Entity-less static bodies via `AddSimulatedBodies` (§3.4) |
| Static kit geometry | Per chunk | **S** | **Merged offline** into section meshes (§3.4) |
| Collidable props, doors, interactables | Per chunk | **G** | Prefab spawnable (entities) |
| Decorative props, decals | Per chunk | **D** 128 / 192 m | Instanced mesh handles |
| Road graph | **Global** | — | Routing, prediction |
| Road surface mesh | Per chunk | **S** | Part of merged sections |
| Navmesh tiles (P3) | Per chunk | **G** | Recast exposes only whole-area rebuilds publicly. Per-chunk tiles need our own `addTile`/`removeTile` on the raw `dtNavMesh` (◐ V-79) |
| Seamless interior content | Per interior | **I** 48 / 64 m from portal | — |
| Airlock / vignette cells | Per cell | Trigger (preload at 60 m) | — |
| Triggers / volumes | Per chunk | **G** | Mission triggers are global |
| Local lights | Per chunk | **G** | §3.8 caps |
| Reflection probes | Per chunk, per lighting state | **S**, current state only | §3.9 |
| Sun/sky/fog/TOD presets | **Global** | — | — |
| Audio emitters/zones | Per chunk | **G** | — |
| HLOD T1 proxy + coarse collision | Per chunk | **H1** 2 000 / 2 200 m | §3.4 fallback |
| HLOD T2 + skyline | **Global** | — | — |
| Water planes, flood level | **Global** | — | — |
| Global heightmap (2 m) | **Global** | — | 5.3 MB |
| Chunk descriptor table | **Global** | — | ~64 KB |
| AD-09 variant groups | In G/S data | — | Evaluated at instantiation; re-evaluated in place |

**Terrain decision:** per-chunk mesh tiles + entity-less cooked collision, not the O3DE Terrain gem.
- *Source check:* Terrain Layer Spawner components register/unregister their area in Activate/Deactivate, so they **could** be spawned with chunk prefabs. They require box shapes, geometry is sector LOD grids, and clipmaps are for materials only (◐ V-78).
- *Why we still choose mesh tiles:* the cost of terrain-system re-registration at 2.6 chunks/s is unmeasured, and mesh tiles keep all cost under our scheduler.
- *Revisit:* P2 week 6, if terrain tile authoring proves expensive.

---

## 3.3 Chunk Lifecycle State Machine (per chunk-layer)

### States and their engine mapping

| State | Meaning | Thread | Engine mechanism (26.10) |
|---|---|---|---|
| `Unloaded` | Descriptor only | — | — |
| `Queued` | Wanted, in the priority queue | Main | — |
| `IOPending` | Assets requested | AssetManager `LoadAssetJob` (legacy JobManager) (✔ V-36) | `Asset<T>` requests for the G/I spawnable + ChunkData asset (custom `AssetHandler`, parse in `LoadAssetData` on the job thread) (✔ V-31) |
| `Deserializing` | **Post-load preparation** | **TaskGraph** job (✔ V-38) | Variant selection from a global-state snapshot; spawn-batch index lists; transform tables; physics `CreateShape` from cooked `.pxmesh` data [VERIFY CreateShape thread-safety, E4] |
| `LoadedInactive` | Data resident, nothing in the world | — | Assets held + arena filled. The engine has **no "spawn inactive"** option (✔ V-37), so this state is pre-spawn by design |
| `Instantiating` | Entering the world, sliced | **Main** | Per frame: `SpawnEntities(ticket, batchIndices)` (≤ 10 entities per batch), `AcquireMesh` batches, `AddSimulatedBodies` batch |
| `Active` | In the world | — | — |
| `Deactivating` | Leaving the world, sliced | **Main** | `OnLayerDeactivating` notify → delta flush → `DespawnEntity` batches → `ReleaseMesh` batches → `RemoveSimulatedBodies` |
| `Unloading` | Release | Main | Drop `Asset<T>` refs; return arena blocks |
| `Failed` | Fallback engaged | — | Coarse proxy bodies from H1 |

**Asset-ready notifications arrive on the main thread** via `AssetManager::DispatchEvents` in the system tick (✔ V-36). That's where `IOPending → Deserializing` is triggered.

### Tick order and spawn slicing (verified engine order)

| Order | Who | What |
|---|---|---|
| 75 `TICK_INPUT` | Engine | Input |
| **78** | **StreamingSystemComponent** | Scheduler. Queue this frame's spawn batch(es) and handle acquires. Batch-add/remove static bodies |
| 80 `TICK_GAME` | SpawnableSystemComponent | `ProcessQueue(High \| Regular)` executes the queued batches **this frame** (✔ V-37) |
| 200 `TICK_PHYSICS_SYSTEM` | PhysX | Simulate; bodies added at 78 are present this step (✔ V-44) |

Rules:
- **Never queue more than one frame's batch ahead per ticket.** A queued spawn can't be cancelled (✔ V-37), so at most one batch is ever "in flight", which keeps cancellation latency to ≤ 1 frame.
- Spawn priority: pinned and G layers within 128 m use `SpawnablePriority` ≤ 64 (the high lane, which also runs when the window is unfocused). Everything else uses 128 (Default).
- Batch boundaries respect entity hierarchy: a batch contains whole root-to-leaf groups. Cross-batch entity references are a [VERIFY] (E1 spike: does id remapping hold across `SpawnEntities` calls on the same ticket?). The fallback is one small spawnable per layer *section*.
- Focus is consumed from the previous frame (≤ 1 frame ≈ 0.9 m latency at 55 m/s; absorbed by radii).
- Completion callbacks are written thread-agnostic: they fire on the main thread today, but the interface allows otherwise (✔ V-37).

### Text state diagram

```text
                    want (dist ≤ R_in, or pinned)
   ┌──────────┐ ───────────────────────────────▶ ┌────────┐
   │ Unloaded │                                   │ Queued │──cancel (dist > R_out & !pinned)──▶ Unloaded
   └──────────┘ ◀──────────────┐                  └───┬────┘
        ▲                      │                      │ dequeued (load slots + bytes-in-flight available)
        │                 ┌────┴─────┐            ┌───▼───────┐
        │                 │ Unloading│◀──cancel───│ IOPending │──timeout/err (retry ≤2, backoff 0.5 s, 2 s)──▶ Failed
        │                 └────▲─────┘  (drop     └───┬───────┘
        │                      │        refs)         │ OnAssetReady (main, DispatchEvents)
        │                      │                  ┌───▼──────────┐
        │                      ├──────cancel──────│ Deserializing│──validation err──▶ Failed
        │                      │                  └───┬──────────┘  (TaskGraph post-load job)
        │                      │                      │ job done (completion queue → main)
        │   evict (LRU / dist  │                  ┌───▼───────────┐
        └──── > R_evict) ──────┴──────────────────│ LoadedInactive│◀──────────────┐
                                                  └───┬───────────┘               │
                                                      │ want active               │ done
                                                  ┌───▼──────────┐          ┌─────┴───────┐
                                                  │ Instantiating│─abort*──▶│ Deactivating│
                                                  └───┬──────────┘          └─────▲───────┘
                                                      │ last batch complete       │ dist > R_out & !pinned
                                                  ┌───▼────┐                      │ (+ min dwell 3 s)
                                                  │ Active │──────────────────────┘
                                                  └────────┘
   Failed ──(retry after 10 s, or leaves R_out)──▶ Unloading
   * abort = stop issuing batches; the ≤1 in-flight batch completes; then despawn what exists
```

### Transition rules (invariants)
1. Only the main-thread scheduler writes state.
2. `Active` only from `Instantiating`; `Instantiating` only from `LoadedInactive`.
3. Pinned layers never cancel, deactivate or evict.
4. Generation increments once per load cycle (on `IOPending` entry).
5. A chunk's G layer must be `Active` before its I/D layers instantiate.
6. **At most one spawn batch in flight per ticket.**

### Hysteresis [T]

| Layer | R_in | R_out | R_evict | Min dwell |
|---|---|---|---|---|
| G | 256 m | 320 m | 512 m | 3 s |
| S | 384 m | 448 m | 640 m | 3 s |
| D | 128 m | 192 m | 320 m | 2 s |
| I | 48 m (portal) | 64 m | 128 m | 2 s |
| H1 | 2 000 m | 2 200 m | 2 400 m | 5 s |

### Priority function

```text
d        = distance(focus, chunkAABB)
v_along  = max(0, dot(velocity, dirToChunk))
eta_lin  = d / max(v_along, 1.5)
eta_road = pathDistanceAlongRoadPrediction / max(speed, 1.5)     // only if on the predicted path (3 s horizon)
eta      = min(eta_lin, eta_road)
key      = (pinClass, eta × layerFactor)                          // G 1.0, I 1.0, S 1.5, D 3.0, H1 4.0
R_in_eff = R_in + |v| · 3 s · max(0, cosθ)                        // +165 m forward at 55 m/s
```

Engine priority mapping:
- `key` order decides which layers get load slots.
- Spawn lane: high (≤ 64) for pinClass 0 or G with eta < 2 s, otherwise Default (128).
- Asset load priority/deadline is passed through `AssetLoadParameters` where supported [VERIFY deadline/priority fields; the IStreamer itself supports both ✔ V-35].

### Cancellation (engine-accurate)

| From | Mechanism |
|---|---|
| `Queued` | Remove from our queue |
| `IOPending` | **AssetManager has no cancel.** Drop our `Asset<T>` refs. If the load completes anyway, `OnAssetReady` finds a stale generation and releases |
| `Deserializing` | Atomic cancel flag checked between job sections; the arena is released |
| `Instantiating` | Stop issuing batches. The ≤ 1 in-flight batch completes. Then `Deactivating` despawns what exists |
| (direct streamer reads, if ever used) | `IStreamer::Cancel` is best-effort; handle `RequestStatus::Canceled` *and* late completion (✔ V-35) |

The trigger is distance > **R_out**.

### Failure / timeout

| Stage | Timeout [T] | Action |
|---|---|---|
| IOPending | 5 s | Retry ×2 (0.5 s, 2 s) → `Failed` |
| Deserializing | 1 s | Validation failure → `Failed` immediately |
| Instantiating | 10 s | Log + continue (telemetry) |
| `Failed` G layer | — | Enable the coarse collision proxy (H1 data, entity-less bodies). Log. HUD warning. Retry after leaving R_out or after 10 s |

---

## 3.4 Async Asset and I/O Pipeline

### Packaging

| Data | Product | Built by | Runtime path |
|---|---|---|---|
| G, I layers, cells (interactive) | `.prefab` → `.spawnable` | PrefabBuilder (✔ V-27) | `SpawnableEntitiesInterface` with an `EntitySpawnTicket`; batched `SpawnEntities` (✔ V-37) |
| S/D bulk data: section list, instanced-prop tables, cooked collision blobs, variant tables, stable ids | Custom **`.ltchunk`** | Custom builder via `AssetBuilderSDK::AssetBuilderDesc` + `RegisterBuilderInformation` (✔ V-30) | Custom `AZ::Data::AssetHandler` registered with `AssetManager::RegisterHandler` (✔ V-31). Parsing into the chunk arena happens in `LoadAssetData` on the load job thread |
| Merged section meshes, prop meshes, materials, textures | Atom model/material/image products | Asset Processor | `MeshFeatureProcessorInterface::AcquireMesh` (✔ V-51) |
| Cooked collision | `.pxmesh` (✔ V-32) or cooked blobs inside `.ltchunk` (`CookTriangleMeshToMemory`) | AP / our builder | `CreateShape(…, CookedMeshShapeConfiguration)` → `AddSimulatedBodies` (✔ V-68) |
| Release packaging | One .pak per district + global | AssetBundler `seeds`/`assetLists`/`bundles` (✔ V-33) | `GameApplication` opens `@products@/*.pak` (✔ V-33) |

### Static geometry path (decision, revised)

1. **Kit geometry is merged offline.** The `.ltchunk` builder merges every static kit piece in a chunk into **section meshes**: one per (32 m sub-cell × material), with LOD0–3 regenerated per section, 16-bit indices per section, ≤ 200 per urban chunk.
   - *Why:* Atom's per-handle cost (ModelDataInstance + MeshLoader + TransformService id + ObjectSrg + octree entry, walked by `Simulate` every frame, ✔ V-51) makes 4 000 handles per chunk × 64 chunks (≈ 256 k) non-viable.
   - *Cost:* merged geometry is unique per chunk, so it gets more VRAM than shared kits (§3.5). Occluder planes are extracted separately.
2. **Decorative props = instanced handles.**
   - Requires `r_meshInstancingEnabled=true` (**off by default**, ✔ V-52), shared material instances (`Material::FindOrCreate` / same `m_customMaterials`), `m_useForwardPassIblSpecular = false`, and no `SetSortKey`.
   - Instances group by {model, LOD, mesh, material instance, sort key}.
   - With instancing on, `SetDrawItemEnabled` is a no-op, so shadow opt-out is per material (§3.6f).
3. **Every static handle:**
   - `m_isRayTracingEnabled = false` (it defaults to **true**).
   - `SetTransform` once **before** model init (a later change marks the mesh dynamic and adds motion-vector draws).
   - Acquire and release on the main thread only, never overlapping `MeshFeatureProcessor::Simulate`. There are no locks, only a debug concurrency checker (✔ V-51).
4. **Static collision** is entity-less:
   - `StaticRigidBodyConfiguration` + `ShapeVariantData`, then `Scene::AddSimulatedBodies` / `RemoveSimulatedBodies` (✔ V-68).
   - **Main thread only, outside the physics tick.** PxScene takes write locks, but `m_simulatedBodies` is unguarded.
   - Shape creation happens on workers during `Deserializing` [VERIFY].
   - Fallback (R3): prefab path only, with entity budgets −40%.

### Scheduling model (per frame, main thread, tick order 78)

1. **Desired set:** SoA scan with radius and hysteresis.
2. **Prioritise:** ETA keys.
3. **Issue loads:** while `inFlightLayers < 4` and `bytesInFlight + est ≤ 64 MB` [T].
4. **Drain completions:** asset-ready events received in the previous `DispatchEvents`, plus TaskGraph post-load completions.
5. **Dispatch post-load jobs:** ≤ 2 concurrent [T] (TaskGraph). AssetManager and PhysX still use the legacy JobManager pool, so the worker split is tuned in P2 (`cl_taskGraph*` CVars, ✔ V-38).
6. **Instantiate within 1.4 ms:**
   - ≤ 1 spawn batch per ticket (≤ 10 entities); its cost is charged when TICK_GAME processes it, using a measured EMA.
   - `AcquireMesh` batches (≤ 50 handles per item).
   - `AddSimulatedBodies` batches.
7. **Deactivate within 0.4 ms:** `DespawnEntity` batches, `ReleaseMesh` batches, `RemoveSimulatedBodies`.
8. **Publish stats.**

### Priority classes (highest first)
1. Pinned / G
2. VO
3. S
4. Texture mips (Atom streaming priority, `SetStreamingPriority`)
5. D
6. H1 prefetch

### Back-pressure and graceful degradation

| Level | Trigger [T] | Action | Visible effect |
|---|---|---|---|
| L0 | All G slack ≥ 1.5 s | Normal | — |
| L1 | Any G slack < 1.5 s | Suspend D/H1 loads. S drops to class 5. Slice grows to `min(4 ms, 16.6 − lastFrameMsExclStreaming)` | Fewer small props briefly |
| L2 | Any G slack < 0.75 s | Coarse collision proxies (entity-less) for unready G chunks | Proxy detail longer |
| L3 | Any G slack < 0.3 s | Vehicle speed soft-cap `v_cap = max(15, (d_unready − 30)/T_ready_est)` | Car "governed" briefly |
| L4 | Within 16 m of an unready, unproxied G chunk | Freeze + fade until ready. **Logged as a defect** | Stall |

**Fade-in:** Atom has **no dithered/cross-fade LOD** (✖ V-58). S layers appear behind H1 at ≥ 384 m, where the pop is small.

---

## 3.5 C++ Memory Management Strategy

### Allocator architecture

| Allocator | Source | Used for |
|---|---|---|
| `AZ::SystemAllocator` | O3DE (✔ V-39) | Long-lived gem objects via `AZ_CLASS_ALLOCATOR(Type, AZ::SystemAllocator)` |
| **`LowTideStreamingAllocator`** | `AZ_CHILD_ALLOCATOR_WITH_NAME(…, "LowTideStreaming", "{UUID}", AZ::SystemAllocator)` (✔ V-39) | All gem heap allocations, so they show as one named line in `GetAllocatorStats` and allocation records |
| `AZ::ThreadPoolAllocator` / `ThreadPoolBase<T>` | O3DE, **thread-safe** (✔ V-39) | Small objects allocated from post-load jobs |
| `AZ::PoolAllocator` | O3DE, **not thread-safe** (✔ V-39) | Main-thread-only small objects |
| AZStd containers | O3DE | All gem containers; default allocator only in cold paths |
| **ChunkBlockPool** | Ours | 192 MB reserved at boot = 768 × 256 KB blocks [T]; uniform blocks; thread-safe acquire/release |
| **ChunkArena** | Ours | Per chunk-layer linear allocator over pool blocks; holds parsed `.ltchunk` data. Filled by the asset handler's `LoadAssetData` (load job) and the post-load job, with single ownership at each step |
| **FrameScratch** | Ours | Main 1 MB, workers 256 KB |
| **Fixed pools** | Ours | Descriptors (1 024), handle slots (2 048), work items (512) |

Engine-owned objects (entities, mesh handles, physics bodies, assets) are owned through tickets/handles, counted per chunk and released in `Deactivating`/`Unloading`.

### Memory budgets (min spec) [T]

**RAM, 6 144 MB ceiling:**

| Category | MB | Derivation |
|---|---|---|
| Engine/runtime baseline | 1 500 | ASSUMPTION. Measured P2 week 1 with `QueryMemInfo` + `GetAllocatorStats` (✔ V-41) |
| Texture staging / CPU residency | 256 | — |
| Mesh CPU-side buffers | 256 | — |
| Collision (G + proxies) | 256 | 36 × ≤ 6 MB + 40 MB |
| Audio | 192 | — |
| Animation | 256 | — |
| Entities/components | 160 | 36 × 600 prefab entities × ~6 KB + dynamic |
| **Mesh-handle CPU overhead** | 96 | ≤ 24 k handles × ~4 KB (ModelDataInstance, MeshLoader, SRG CPU side) [T], measured E3 |
| Chunk arenas (ChunkBlockPool) | 192 | ~116 resident layers × 1.65 MB |
| LoadedInactive cache | 256 | LRU |
| Streaming buffers | 96 | — |
| Global resident world data | 128 | — |
| Navigation (P3) | 128 | — |
| AI/traffic (P3) | 128 | — |
| UI | 96 | — |
| Game state + mission/dialogue | 64 | — |
| VFX | 96 | — |
| Frame scratch + job temp | 32 | — |
| **Sum** | **4 188** | |
| **Headroom** | **1 956 (32%)** | Reduce to ≤ 15% after measurement |

**VRAM, 4 096 MB ceiling:**

| Category | MB | Derivation |
|---|---|---|
| Render targets + swapchain + shadows | 512 | Unchanged [VERIFY via the RHI memory view ✔ V-62] |
| Texture streaming pool (`r_streamingImagePoolBudgetMb`, ✔ V-59) | 1 450 | 2K set = BaseColor **BC1** 2.8 + Normal **BC5** 5.6 + Roughness **BC4** 2.8 = 11.2 MB (preset formats ✔ V-22). Optional Metal/AO +2.8 each (assume 25% of sets). 40 district sets ≈ 476 + 150 prop sets at 1K ≈ 447 + hero 20 × 14 = 280 + terrain/decals 100 + HLOD 100 ≈ 1 403 |
| **Mesh vertex/index** | **650** | Merged sections are unique per chunk: average urban 200 k LOD0 tris ≈ 120 k verts × 32 B + 600 k × 2 B indices ≈ 5 MB, × 1.5 for LODs = 7.5 MB × 64 S chunks = 480 + props 60 + chars/vehicles 60 + HLOD/terrain 50 |
| Instance/culling buffers + per-handle SRGs | 96 | ≤ 24 k handles |
| Reflection probes | 96 | Bake format is **R9G9B9E5** (4 B/texel, ✔ V-60). Chunk probes at **128²** = 0.52 MB each × 2 × 43 = 45 MB + 20 hero/interior probes at 256² (2.1 MB) = 42 MB |
| VFX | 64 | — |
| UI | 64 | — |
| Water/sky/weather | 64 | — |
| Driver/pools/fragmentation | 300 | RHI pools sub-allocate (D3D12MA/free-list); **no defragmentation** (◐ V-64) |
| **Sum** | **3 296** | |
| **Headroom** | **800 (20%)** | — |

### Ownership and lifetime rules

1. StreamingSystem owns the ChunkTable. Each ChunkLayerRecord owns its arena, `EntitySpawnTicket`, mesh handles, simulated-body handles and `Asset<T>` refs.
2. External references use `ChunkHandle` {ChunkId, layer, generation}. Stale handles resolve to `Unloaded`.
3. Entities are referenced by `AZ::EntityId` and re-resolved every use. **Spawning generates fresh EntityIds** (✔ V-28), so EntityIds never persist.
4. Persistent and cross-chunk references use `StableId` {ChunkId, local u32}.
5. RAII wrappers (`ScopedSpawnTicket`, `ScopedMeshHandle`, `ScopedSimulatedBodies`) release on destruction and assert on the main thread.
6. Cross-border objects belong to the pivot's chunk; bounds tolerance 16 m.

### Hot-path rules
- Zero heap allocations in the scheduler tick (allocation-counter guard in debug).
- Pre-reserved containers; SoA descriptor table.
- `alignas(64)` job outputs; padded atomics.
- No virtual dispatch in the inner scan.
- Profiling: `AZ_DECLARE_BUDGET(LowTideStreaming)` / `AZ_DEFINE_BUDGET` + `AZ_PROFILE_SCOPE(LowTideStreaming, "…")`. The first argument is a declared budget (✔ V-76).

### Fragmentation strategy
1. Uniform ChunkBlockPool.
2. Merged sections + instanced props keep per-object engine allocations bounded.
3. Hysteresis + dwell limit churn.
4. A fixed texture pool.
5. **Engine allocators expose no fragmentation stats** (`GetUnAllocatedMemory()` returns 0, ◐ V-40). The soak proxy is the trend of `reserved − used` per allocator from `GetAllocatorStats`, plus the working-set slope.

### Leak and corruption detection
- Per-chunk tags and ChunkBudgetTracker counters (entities, mesh handles, simulated bodies, asset refs, arena blocks). All must be zero at `Unloaded`.
- **Allocation records:**
  - Enable with `allocator_tracking_LowTideStreaming=<Mode>` (and per-allocator modes) in a `--startup-cfg-file`, or with `AllocatorManager::SetTrackingForAllocator`.
  - Dump with `sys_DumpAllocationRecordsToFile`.
  - Compiled out in `_RELEASE` (◐ V-40).
- **ASan:** `LY_BUILD_WITH_ADDRESS_SANITIZER=ON` (✔ V-16) on core + gem tests + a 10-minute route weekly. Custom pools poison/unpoison blocks.
- Debug guard words on arena blocks.

### Threading model

| Data / operation | Writer | Readers | Synchronisation |
|---|---|---|---|
| SoA state table | Main | Main, HUD snapshot | Single writer |
| `.ltchunk` parse into arena | AssetManager load job (`LoadAssetData`) | — | Exclusive until `OnAssetReady` on main |
| Post-load job input/output | Main → TaskGraph job → main | — | Ownership transfer; completion queue (mutex) |
| Cancel flags | Main | Jobs | `std::atomic<bool>` |
| Stats counters | Any | HUD | Relaxed atomics, padded |
| `SpawnEntities` / `DespawnEntity` requests | Main (tick 78) | Processed main (tick 80) | Engine queue (mutex-protected internally, ✔ V-37) |
| `AcquireMesh`/`ReleaseMesh`/setters | **Main only, not during Simulate** | — | None in engine (✔ V-51) |
| `AddSimulatedBodies`/`RemoveSimulatedBodies` | **Main only, outside physics tick** | — | Engine: PxScene write lock; body list unguarded (✔ V-68) |
| `CreateShape` from cooked data | Post-load job | — | [VERIFY thread-safety, E4] |
| EBus (ours) | Main only | — | Single-threaded bus (asserted) |

---

## 3.6 Culling Pipeline (cheapest first)

| Layer | Provided by (26.10) | We build | Savings | Failure modes → mitigation |
|---|---|---|---|---|
| (a) Streaming cull | — | Layer radii | Very high | Pop-in → ETA priority, H1 swap at ≥ 384 m |
| (b) Frustum | **Atom `CullingScene` over `IVisibilityScene` (octree)** (✔ V-53) | Correct section bounds (32 m sub-cells keep frustum granularity after merging) | High | Oversized merged sections → cap at 32 m cells |
| (c) Occlusion | **Only `OcclusionCullingPlane` quads occlude**, rasterized by Masked Occlusion Culling at a fixed 1920×1080. Meshes never occlude. No plane cap, no CVar (◐ V-54) | **Occluder planes authored on kit pieces**, extracted by the builder into per-chunk occluder lists and registered when S is Active; ≤ 32 active by projected area [T] [VERIFY plane create/enable API from gem code] | High in dense urban | Over-cull → conservative inset 0.5 m. Missing planes → budget report flags urban chunks with < 8 |
| (d) Small-object / screen-size | Atom per-mesh `m_minimumScreenCoverage` (culls below it, ✔ V-55) + D radius | Per-class settings (§3.7) | Medium | Popping → D-layer radius fade inside 128 m |
| (e) Interior/exterior | — | Portal test (frustum ∧ ≤ 64 m ∧ facing) toggling interior visibility (`SetVisible` on interior handles, visibility of interior entities) | High indoors | Openings not on the portal list → validator |
| (f) Shadow casters | Per-cascade shadow views (≤ 4, ✔ V-56). **No shadow-specific LOD.** Opt-out is the **material** property `castShadows` (`SetDrawItemEnabled` is a no-op with instancing) | **Build-time** caster policy: detail props, HLOD T1/T2 and ground cover use `MI_*_NoShadow` material variants. Local shadowed lights ≤ 2/chunk, ≤ 8 global | High | Distance-based per-instance shadow cutoff isn't available; accepted |

**Occlusion decision:** authored occluder planes on kit pieces + engine MOC.
- *Rejected:* GPU Hi-Z (render-pipeline work); mesh occluders (not supported by the engine); PVS (incompatible with streaming).

---

## 3.7 LOD Strategy

**Engine model (✔ V-55, ✖ V-58, ◐ V-56):**
- LOD selection is **per mesh** through `RPI::Cullable::LodConfiguration`: `m_lodType` (Default / ScreenCoverage / SpecificLod), `m_lodOverride`, `m_minimumScreenCoverage` (default 1/1080), `m_qualityDecayRate` (default 0.5).
- Per-LOD coverage ranges are derived internally from the decay rate [VERIFY the exact derivation in `MeshFeatureProcessor.cpp`; E3 records switch distances].
- There is **no per-LOD threshold, no hysteresis, no cross-fade, and no shadow LOD**. Shadow views pick the LOD by their own coverage.
- **Consequence:** LOD popping is accepted; the decay rate per class is tuned to push switches out to distances where the pop is ≤ ~2 px.

| Class | LOD0 tris (max) | LOD1 / LOD2 / LOD3 (authoring ratios) | `minimumScreenCoverage` [T] | `qualityDecayRate` [T] | Beyond |
|---|---|---|---|---|---|
| Merged kit section (32 m) | 60 k | 50% / 25% / 10% | 0 (never culled; H1 takes over) | 0.6 | H1 |
| Unique building (merged) | 40 k | 50% / 25% / 10% | 0 | 0.6 | H1 |
| Prop (instanced) | 3 k (hero 8 k) | 40% / 15% / — | 0.005 | 0.5 | Culled |
| Vehicle (player/hero) | 80 k | 50% / 20% / 5% | 0.004 | 0.7 | Culled |
| Vehicle (traffic) | 40 k | 50% / 20% / 5% | 0.004 | 0.5 | Culled |
| Character (P3) | 50 k / 20 k | 50% / 20% / 8% | 0.004 | 0.6 | Culled |
| Terrain tile | 32 k / 8 k / 2 k / 512 | — | 0 | — | `LodType::SpecificLod` set by the streamer per ring (≤ 128 / 256 / 384 m / H1) via `SetMeshLodConfiguration` (✔ V-51) |
| Tree (instanced) | 15 k | 5 k / 1.5 k / cross-card | 0.01 | 0.5 | Culled / H1 canopy |
| Ground cover | 200 per clump | — | 0.02 | — | D radius ≤ 40 m |

**HLOD** (our own swap, not Atom LOD):
- **T1 per chunk:** ≤ 4 k tris + 512² atlas + coarse collision. Shown when S isn't Active and the chunk's cluster is within 1 000 m.
- **T2 per 4×4 cluster:** ≤ 20 k tris + 1K atlas, beyond 1 000 m. Always resident, together with the skyline.
- HLOD meshes use `_NoShadow` materials.
- Draws: T1 ≤ 164, T2 ≤ 40.
- Generated offline by a Blender script (P4).

**Material/texture LOD:**
- LOD2+ `_Far` material variants are **not available**: a material is per mesh/sub-mesh slot, and LOD selection is internal. *Dropped.*
- Mip streaming within the pool. Hero textures in inspect mode get `SetTargetMip(0)` + high `SetStreamingPriority`. There is **no hard residency pin** (◐ V-59), so a brief blur on first inspect is accepted.

**Authoring pipeline (Blender → O3DE):** node naming is verified (✔ V-20).
- **Base mesh node = LOD0** (no suffix).
- `<Name>_LOD1` … `_LOD3` (the engine matches `_lod1`…`_lod5` case-insensitively).
- Collision node `<Name>_Phys` (matches `_phys`).
- 1 unit = 1 m, +Z up, transforms applied, triangulated.
- Sidecar `.ltmeta` manifest + committed `.assetinfo` (JSON scene manifest, ✔ V-21).

**Validation (`tools/validate_lods.py`, spec):**
- LOD count per class; each LOD ≤ 60% of the previous one; LOD0 cap.
- `_Phys` present for collidable classes (primitives or ≤ 1 hull of ≤ 32 verts for props).
- Material slot cap; `_NoShadow` material on detail/HLOD classes.
- Bounds within the border tolerance.

---

## 3.8 Map Optimization Rules (per chunk, by district profile) [T]

**Frame-level derivation:**
- Draw budget 3 000 = ~12 visible full-detail chunks × 200 + H1 164 + H2 40 + dynamic ~300 + margin.
- Visible triangles ≈ 6 M.
- Mesh handles ≤ 24 k resident (§3.2).

| Rule | Dense urban | Industrial | Rural | Water | Interior cell |
|---|---|---|---|---|---|
| Draw calls (all layers, near) | ≤ 250 | ≤ 180 | ≤ 120 | ≤ 30 | ≤ 200 |
| LOD0 triangles (sum) | ≤ 500 k | ≤ 400 k | ≤ 400 k | ≤ 50 k | ≤ 300 k |
| GPU geometry (all LODs, unique) | ≤ 12 MB | ≤ 10 MB | ≤ 8 MB | ≤ 2 MB | ≤ 10 MB |
| Unique materials | ≤ 48 (district palette ≤ 120) | ≤ 32 | ≤ 24 | ≤ 8 | ≤ 40 |
| Unique texture sets beyond district trims | ≤ 4 | ≤ 4 | ≤ 2 | ≤ 1 | ≤ 6 |
| G prefab entities | ≤ 600 | ≤ 400 | ≤ 200 | ≤ 50 | ≤ 400 |
| S merged-section handles | ≤ 200 | ≤ 150 | ≤ 120 | ≤ 20 | ≤ 150 |
| D instanced handles | ≤ 250 | ≤ 200 | ≤ 400 | ≤ 20 | ≤ 200 |
| Local lights unshadowed / shadowed | ≤ 24 / ≤ 2 | ≤ 16 / ≤ 2 | ≤ 8 / ≤ 1 | ≤ 4 / 0 | ≤ 16 / ≤ 2 |
| Static collision | ≤ 20 k tris + ≤ 200 primitives | same | ≤ 10 k + 100 | ≤ 1 k | ≤ 15 k + 150 |
| Occluder planes | ≥ 8 | ≥ 4 | 0 | 0 | portals |
| Audio emitters / reverb zones | ≤ 16 / ≤ 4 | ≤ 16 / ≤ 2 | ≤ 12 / ≤ 1 | ≤ 4 / 0 | ≤ 12 / ≤ 4 |
| Disk payload G / S / D | ≤ 6 / 8 / 2 MB | same | ≤ 4 / 8 / 3 MB | ≤ 1 / 2 / 0 MB | ≤ 6 / 8 / 2 MB |
| Arena per layer | ≤ 4 MB | ≤ 4 MB | ≤ 4 MB | ≤ 1 MB | ≤ 4 MB |
| Variant sites | ≤ 3 | ≤ 3 | ≤ 2 | 0 | ≤ 3 |

**Rules:**
1. **Static kit pieces are always merged** into 32 m sections by the builder; never one handle per piece.
2. **Instancing:** project setting `r_meshInstancingEnabled=true`. Props share material instances. No unique runtime material instances on static content (`Material::Create` breaks instancing, ◐ V-84). Unique instances are allowed only on vehicles (paint).
3. Modular reuse ≥ 70% of façade area (authoring), which still pays off in authoring time and texture memory after merging.
4. **Textures:**
   - StandardPBR samples **only `.r`** for roughness, metallic and occlusion (✔ source: `RoughnessInput.azsli:49`, `MetallicInput.azsli:46`, `OcclusionInput.azsli:43`). **No channel packing.**
   - Maps: `_BC`, `_N`, `_Rough` (mandatory), `_Metal` and `_AO` (only where they vary spatially; otherwise use material factors + SSAO), `_E`, `_H`, `_O`, `_Mask`.
   - District trim sheets at 2K. > 2K needs a waiver (hero 4K: Tyler, Lena, Mack, Rafe, vessel interior, Mack's car).
5. **Lighting:**
   - No lightmaps.
   - Dynamic sun + per-chunk probes per state + emissive + capped dynamic lights.
   - Atom uses tiled light culling with **256 lights per tile max** (overflow flickers, ◐ V-57), so the per-chunk caps above keep tiles far below it.
   - Only the player's headlights cast shadows; AI headlights ≤ 12 lit.
   - DiffuseProbeGrid **not used** (real-time needs ray tracing; baked mode is a P4 evaluation, ✔ V-61).
6. Collision seams welded by the tool. No triangle meshes on dynamic objects.
7. Waivers in `DT_ChunkWaivers.json` (vessel materials ×2, evacuation entities ×1.5, Mack's workshop lights ×1.5).

**Budget report tool (`tools/chunk_budget_report.py`, spec):**
- **Inputs:** prefabs, `.ltmeta`, `.ltchunk` builder stats, waivers, `DT_ChunkBudgets.json`.
- **Per chunk:**
  - estimated draws; LOD0 tris; GPU geometry MB
  - S/D handle counts; prefab entities
  - materials, texture sets + VRAM
  - lights; collision; occluder planes
  - disk per layer; arena; variants
- **Output:**
  - CSV + Markdown (worst 20, district totals)
  - exit 1 when over budget without a waiver
- **When it runs:** pre-push when `Assets/World/` changes.
- **Runtime cross-check (> 15% divergence = tool bug):**
  - draw items from `PassSystemInterface::GetFrameStatistics` (✔ V-63)
  - triangles from pipeline statistics queries

---

## 3.9 Day/Night, Weather and Crisis Hooks (streaming/memory only)

**Always resident (global):**
- TOD state + sun/sky/fog presets.
- Weather state; **flood level**.
- Camera-attached rain; wetness constant; weather audio; lightning.
- Water planes; global heightmap (5.3 MB).

**Per chunk:**
- Reflection probes for the **current lighting state only**: 3 baked sets (Day, Twilight, Night; Storm reuses Night + tint).
- Each chunk probe is 128² R9G9B9E5 ≈ 0.52 MB. Per chunk on disk: 3 states × 2 × 0.52 ≈ 3.1 MB.
- Transition load at a fade: ~43 S chunks × 2 × 0.52 MB ≈ 45 MB, ≤ 0.3 s.

**Act III flooding (AD-08):**
1. A camera-centred water disc at `floodLevel`. Depth hides it where the ground is higher.
2. A global per-frame constant (`floodLevel`, `wetness`) is reserved for materials (shader work is P4).
3. Gameplay depth comes from a physics raycast (`RayCastRequest` + `QueryScene`, ✔ V-74) when G is Active, or from the global heightmap.
4. Act III changes = AD-09 variant groups re-evaluated in place + road-graph closures. **No reloads.**

---

## 3.10 Save/Load Interaction

**Boundary:** a save = global state sections + chunk delta store. No live entity state is saved; saves happen only at safehouses/checkpoints with heat = 0.

| Scope | Content | Persisted when |
|---|---|---|
| Global | 01 §1.2 global rows | Save |
| Chunk-local | DeltaRecord {StableLocalId, kind, value, optional chunk-local transform} | Recorded at the event; flushed from Active layers on save; persists across unload |

Delta cap 256 per chunk [T]. Cosmetic kinds are evicted first; gameplay kinds never.

**Format and versioning:**
- Header `{'LTSV', fileVersion, gameVersion, timestamp, CRC32}` + TLV sections `{id, version, size, payload}`.
- Per-section migration chain; unknown sections skipped; missing sections default.
- `ChunkContentHash` per chunk: deltas whose ids vanished are dropped and logged.
- **Custom binary TLV.** SerializeContext versioning exists (`ClassBuilder::Version(n, VersionConverter)`, ✔ V-85), but it is still rejected for saves to keep the save format independent of engine reflection churn.

**Interface (declarations only; headers in `tools/templates/LowTideStreaming/`):**

```cpp
namespace LowTide::Save
{
    using SectionId = uint32_t;

    // Implemented by each global-state owner. Main thread only.
    class ISaveParticipant
    {
    public:
        virtual ~ISaveParticipant() = default;
        virtual SectionId GetSectionId() const = 0;
        virtual uint16_t GetSectionVersion() const = 0;
        virtual bool Write(class SaveWriter& writer) const = 0;
        virtual bool Read(class SaveReader& reader, uint16_t storedVersion) = 0; // migrates if older
        virtual void ResetToDefaults() = 0;                                       // section absent
    };
}
// Chunk delta store: see IChunkDeltaStore.h
```
