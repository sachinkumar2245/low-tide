# FILE: /docs/04_World_Streaming_Design.md

**Project:** LOW TIDE | **Phase:** 1 | **Doc status:** v0.1 | **Engine:** O3DE 26.05.x (03 §2.1)

Labels:
- **[T]** = tunable starting value, validated by the P2 testbed (05) and P3 profiling.
- **[VERIFY]** = engine claim to confirm (index in 06 §4.6).
- ASSUMPTION lines are inputs that haven't been measured yet.

---

## 3.0 Targets and Frame Budgets

### Minimum spec (defined here)

| Component | Min spec | Justification |
|---|---|---|
| CPU | 6C/12T, 2019-class (Ryzen 5 3600 / Core i5-10400) | Covers main + render submission + I/O + ≥3 job workers (§3.5 threading) |
| RAM | 16 GB | 6 GB process ceiling plus OS, browser and launcher overhead |
| GPU | 6 GB VRAM, GTX 1660 Super / RX 5600 XT class, DX12 FL 12_0 [VERIFY Atom minimum feature level] | 4 GB budget + ~2 GB OS/compositor/driver margin |
| Storage | **SATA SSD (≥ 450 MB/s seq). HDD unsupported** | The streaming math (§3.2) assumes ≥ 150 MB/s *effective* small-block throughput. HDD random reads (~1–5 MB/s effective) can't meet it |
| OS | Windows 10 22H2 / Windows 11 | O3DE floor is 20H2 |

### Frame budget at 60 FPS (16.6 ms) on min spec, 1080p [T]

| Thread / unit | Budget (ms) | Contents |
|---|---|---|
| **Main (game) thread** | **16.6 total** | Game logic + AI (P3) 4.0 · physics step/sync 2.5 · animation sync 1.0 · **streaming 2.0** (scheduler 0.2 + instantiation 1.5 + deactivation 0.3) · UI 0.5 · audio 0.3 · engine tick/EBus overhead 2.0 · **slack 4.3** |
| **Render CPU** (Atom RPI/RHI submission + job-recorded command lists) [VERIFY Atom render threading model] | ≤ 12.0 | ≤ 3000 main-view draws + ≤ 1500 shadow draws (§3.8 derivation) |
| **GPU** | 16.6 | Depth pre/opaque 6.0 · shadows 3.0 · transparency/water 1.5 · post (TAA, tonemap, bloom) 2.0 · other 2.0 · slack 2.1 |
| **Streaming workers** | Background | ≤ 2 concurrent deserialize jobs, each ≤ 8 ms. I/O in flight ≤ 64 MB |

---

## 3.1 World Coordinate System

### Precision analysis

float32 has a 24-bit significand. For |x| in [2^e, 2^(e+1)), the spacing between representable values is ULP = 2^(e−23).

| Distance from origin | ULP | Smallest motion per 60 Hz step (ULP×60) | Pixel size at 0.5 m camera distance (1080p, 60° vFOV)* |
|---|---|---|---|
| 1 024–2 048 m | 0.122 mm | 7.3 mm/s | 0.53 mm |
| **2 048–4 096 m** | **0.244 mm** | **14.6 mm/s** | 0.53 mm |
| 4 096–8 192 m | 0.488 mm | 29.3 mm/s | 0.53 mm |
| 8 192–16 384 m | 0.977 mm | 58.6 mm/s | 0.53 mm |

\*Pixel footprint = 2·0.5·tan(30°)/1080 ≈ 0.53 mm. This is the inspect-mode close-up worst case (V29).

**Safety criterion:** ULP ≤ 0.25 mm. The reasons:
1. Vertex quantization stays under half a pixel in the closest camera case (inspect mode at 0.5 m).
2. The slowest resolvable motion (14.6 mm/s) stays below typical rigid-body sleep velocities, so creeping motion is quantized only when bodies are about to sleep anyway [VERIFY PhysX 5 default sleep threshold in O3DE].

**→ Maximum safe distance from origin = 4 096 m** (the last binade where ULP = 0.244 mm).

### World extents check
- World grid = **32 × 32 chunks × 128 m = 4 096 m square**, origin at the grid centre. Coordinates span [−2 048, +2 048] m on X and Y.
- Worst point (corner): √(2048² + 2048²) = **2 896 m < 4 096 m** ✓.
- Vertical range is −20 m (sea floor proxy) to +250 m (skyline).
- The full-vision map (4 × 4 km) fits. The shippable map (~3 × 2.5 km plus the offshore vessel chunk) is a subset.
- The grid can grow to 5.79 km square (half-diagonal = 4 096 m) before violating the criterion.

### Decision: single fixed origin, float32 world positions, chunk-local authored coordinates

| Aspect | Rule |
|---|---|
| Runtime positions | Plain float32 world space (engine native) |
| Origin | Fixed at the grid centre. **No origin shifting** |
| Authored data | Chunk content is stored **relative to the chunk origin**. The chunk origin = integer grid coordinate × 128 m, which is exact in float. Chunks stay relocatable for the generator and testbed |
| Guardrail | Debug/profile assert if any entity's \|x\| or \|y\| exceeds 3 000 m. Playable bounds are enforced by world-edge collision |
| Time | Game clock is a **double** (accumulated seconds). Shader time is wrapped every 3 600 s, because float seconds lose precision after ~24 h (ULP ≈ 7.8 ms at 86 400 s) |

Engine interaction:
- O3DE `AZ::Vector3` / `AZ::Transform` are float32, and there is no double-precision world build option [VERIFY].
- PhysX has a native origin-shift API, but O3DE's PhysX gem doesn't expose it as a supported workflow [VERIFY].
- Atom uses float world-space positions in its view/object constants [VERIFY whether camera-relative rendering exists].
- None of this matters inside 4 096 m. That's why this decision needs zero engine changes.

**Rejected:**
- Floating origin: no engine support. Every system (physics scene, render, audio, AI, particles) would need rebase handling, which is unnecessary at < 4 km.
- Double-precision positions: an engine-wide type change, and not supported.

---

## 3.2 Spatial Partitioning

### Decision: uniform 128 m grid for streaming + two-tier HLOD hierarchy (hierarchical grid)

| Option | Verdict |
|---|---|
| **Uniform grid, 128 m chunks, per-layer radii, HLOD tiers T1 (per chunk) and T2 (4×4-chunk clusters)** | **Selected.** O(1) coordinate→chunk mapping, trivial naming and generation, predictable load units, simple neighbour and seam rules |
| Quadtree (adaptive cells) | Rejected: variable load units make budgeting and time-slicing unpredictable. Density adaptation is handled by per-district budgets instead |
| Single-level grid without HLOD | Rejected: no long-distance visibility (prologue skyline, rural sight-lines, open water) |

### Chunk size derivation (128 m)

Inputs (ASSUMPTION unless stated):

| Input | Value | Source |
|---|---|---|
| v_max | 55 m/s (fact, AD-06) | 01 |
| Effective read throughput, min spec SATA SSD | 150 MB/s [T] | 33% of the 450 MB/s sequential rate (small files, decompression, contention) |
| Gameplay-layer payload per chunk (worst urban) | 6 MB [T] | Collision + gameplay prefab + chunk data (§3.8) |
| Static-visual payload per chunk (worst) | 8 MB unique [T] | Unique meshes + probes. Shared kit textures are mostly already resident |
| Entity activation cost (prefab path) | 25 µs/entity [T] | Unmeasured. **P2 testbed measures this first** |
| Entities per urban gameplay layer | ≤ 600 (§3.8) | Budget |
| Main-thread instantiation slice | 1.5 ms/frame | §3.0 |

Per-chunk gameplay-layer ready time (worst):
- I/O: 6 MB / 150 MB/s = **40 ms**
- Deserialize (worker): **≈ 30 ms** [T]
- Instantiate: 600 × 25 µs = 15 ms of main-thread work at 1.5 ms/frame = 10 frames = **167 ms**
- **T_chunk ≈ 0.24 s**

Size comparison at an equal minimum lead distance (gameplay layer R_in = 256 m, static R_in = 384 m):

| Chunk size | Chunks resident (static layer, worst) | Per-chunk instantiation (urban) | Verdict |
|---|---|---|---|
| 64 m | ~169 (13×13) | ~4 ms | Per-chunk overhead dominates: descriptor, prefab spawn, I/O request count, instancing batches split per chunk, seam count ×2 |
| **128 m** | **~64 (8×8 worst; ~43 euclidean)** | **~15 ms (10 frames)** | **Matches the ~100–150 m city-block authoring unit. Bounded load spikes** |
| 256 m | ~16 | ~60 ms (40 frames) | Resident area +31% from coarse granularity. Coarse cancellation. Prefabs > 2 000 entities are slow to edit [VERIFY editor prefab scaling] |

Resident-area waste for a ring of minimum distance D = (2D + S)². At D = 384 m: S=64 → 0.69 km², S=128 → 0.80 km², S=256 → 1.05 km².

### Stream-in radius vs. speed vs. load time

Straight-line travel at v_max through dense urban (worst content):

| Quantity | Gameplay layer (R_in 256 m) | Static-visual layer (R_in 384 m) |
|---|---|---|
| New chunks entering radius per 128 m travelled ≈ 2(R_in + 64)/128 + 1 | **6** | **8** |
| Time to travel 128 m at 55 m/s | 2.33 s | 2.33 s |
| Demand (chunks/s) | 2.6 | 3.4 |
| I/O demand | 2.6 × 6 MB = 15.6 MB/s | 3.4 × 8 MB = 27 MB/s |
| Main-thread demand | 2.6 × 15 ms = 39 ms/s | 3.4 × 6 ms (bulk path) = 20 ms/s |
| **Combined** | **I/O 43 MB/s = 29% of 150 MB/s.** Main **59 ms/s = 0.98 ms/frame = 65% of the 1.5 ms slice** ✓ | |
| Diagonal travel (×1.41 chunk crossings) | I/O 41%, main **1.38 ms/frame = 92%** ⚠ worst sustained case | |

Lead-time check (gameplay layer):
- Time to contact from R_in = 256 m / 55 m/s = **4.65 s**.
- Worst ready time = queue (6 chunks ahead × 0.167 s serial instantiation = 1.0 s) + T_chunk 0.24 s = **1.24 s**.
- **Safety factor = 3.7.**
- Minimum admissible radius: R_in ≥ v_max × T_ready × SF(2) = 55 × 1.24 × 2 = **136 m**. The 256 m choice survives load times up to 3.7× worse than estimated before degradation engages (§3.4).
- Velocity look-ahead (§3.3) adds a further +v·3 s = 165 m along the travel direction.

Synchronous loads (save-load, mission restart, vignette exit) behind a fade:
- I/O: 36 × 6 MB + 64 × 8 MB = 728 MB → 4.9 s.
- Main-thread instantiation (unsliced): ≈ 0.9 s.
- **≈ 6 s**, which meets MVS D9 (≤ 30 s cold load).

### Grid, coordinates and density profiles

| Item | Value |
|---|---|
| Grid | 32 × 32 = 1 024 cells. `Chunk_X000_Y000` = SW corner. World min corner of chunk (x,y) = ((x−16)·128, (y−16)·128) |
| Shippable occupancy | ~4.8 km² land + water ≈ 300–350 non-empty chunks. Empty sea cells hold water only (global) |
| Prefab/level constraint | The runtime level `L_World` contains **only global entities**. Chunk layers are separate spawnables spawned by the streamer. Authoring uses district context levels (`L_Author_OM`) that instance neighbouring chunk prefabs read-only [VERIFY prefab edit/focus-mode workflow for nested prefabs]. Per-layer prefab entity cap = 1 000 (editor responsiveness) [T] |

| District profile | Prefab entities (gameplay layer) | Bulk instances (static/detail) | Notes |
|---|---|---|---|
| Dense urban (OM, GM edge) | ≤ 600 | ≤ 4 000 | Occluder-rich, interiors |
| Industrial (TC) | ≤ 400 | ≤ 3 000 | Large repeated volumes (containers) |
| Rural (BC) | ≤ 200 | ≤ 6 000 | Vegetation scatter is bulk-only |
| Water / offshore | ≤ 50 | ≤ 200 | Vessel chunk has an exception (§3.8) |

### Chunk content taxonomy and residency

Each chunk is split into **layers**. Each layer is an independently streamed unit with its own radius. The state machine (§3.3) runs per (chunk, layer).

| Content | Residency | Layer / radius (R_in / R_out) [T] | Notes |
|---|---|---|---|
| Terrain render tile | Per chunk | **S** Static-visual 384 / 448 m | Mesh tiles with LOD0–3 + skirts (§3.7). Terrain gem not used (below) |
| Terrain + static collision | Per chunk | **G** Gameplay 256 / 320 m | Pre-cooked PhysX meshes and primitives |
| Static geometry (kit pieces, unique buildings) | Per chunk | **S** | Entity-less bulk path (§3.4) |
| Collidable props, doors, interactables | Per chunk | **G** | Prefab path (entities) |
| Decorative detail (litter, decals, small non-collidable props) | Per chunk | **D** Detail 128 / 192 m | Bulk path |
| Roads: global road graph (lanes, connectivity, closures) | **Global, always resident** | — | Routing, traffic (P3), path prediction for streaming priority |
| Roads: surface mesh/decals | Per chunk | **S** | — |
| Navmesh tiles | Per chunk (reserved P3) | **G** | [VERIFY RecastNavigation gem tile add/remove at runtime] |
| Seamless interior content | Per interior | **I** 48 / 64 m from portal, or camera inside | The shell exterior is part of S |
| Airlock interior / vignette cells | Per cell | On airlock trigger (preload at 60 m) | Separate spawnables (AD-01, AD-12) |
| Gameplay triggers / volumes | Per chunk | **G** | Mission triggers are mission-owned (global) |
| Local lights | Per chunk | **G** (near lights) | Count caps §3.8 |
| Reflection probes | Per chunk **per lighting state** | **S**, current state only | §3.9 |
| Sun, sky, fog, global ambient, TOD presets | **Global** | — | — |
| Audio emitters, reverb zones | Per chunk | **G** | Global: music, weather beds |
| HLOD T1 (per-chunk proxy) | Per chunk | **H1** 2 000 / 2 200 m | Includes the **coarse collision proxy** (§3.4 fallback) |
| HLOD T2 (4×4 cluster) + skyline (AD-13) | **Global, always resident** | — | ~30 clusters + skyline |
| Global water planes, flood level | **Global** | — | AD-02, AD-08 |
| Global heightmap (2 m) | **Global** | — | 3 × 3.5 km / 2 m = 2.6 M samples × 2 B = **5.3 MB**. Flood depth, AI and audio queries without loaded physics |
| Chunk descriptor table | **Global** | — | 1 024 × ~64 B ≈ 64 KB |
| AD-09 variant groups | Inside G/S layer data | — | Evaluated at instantiation; re-evaluated in place on global change |

**Terrain decision:** per-chunk mesh tiles plus cooked collision, not the O3DE Terrain gem.
- *Why:* our streamer controls every byte. The world is mostly urban.
- *Trade-off:* no gem splatting/clipmaps, and terrain LODs are authored or generated by our tool.
- *Rejected:* Terrain gem, because runtime streaming of terrain spawner areas under our scheduler is unverified [VERIFY].

---

## 3.3 Chunk Lifecycle State Machine (per chunk-layer)

### States

| State | Meaning | Thread performing work | Memory held |
|---|---|---|---|
| `Unloaded` | Descriptor only | — | 0 (descriptor is global) |
| `Queued` | Wanted; waiting in the priority queue | Main (scheduler) | 0 |
| `IOPending` | Asset/I/O requests issued | AssetManager / AZ::IO Streamer threads [VERIFY] | Staging buffers |
| `Deserializing` | ChunkData parsed into the chunk arena; variant selection from a global-state snapshot; delta records matched | Job worker | Arena |
| `LoadedInactive` | Data in RAM, nothing in the world (warm cache) | — | Arena + asset refs |
| `Instantiating` | Spawn prefab entities, acquire bulk render handles, add physics actors, apply deltas | **Main, time-sliced** | Arena + engine objects |
| `Active` | In the world, simulated, visible | — | Full |
| `Deactivating` | Flush deltas to the store, despawn, release handles and actors | **Main, time-sliced** | Shrinking |
| `Unloading` | Release asset refs, return arena blocks | Main (cheap) + asset system | → 0 |
| `Failed` | Retries exhausted; fallback engaged | — | Proxy only |

### Must happen on the main thread
- Entity spawn/despawn (SpawnableEntitiesInterface) [VERIFY threading].
- Render handle acquire/release [VERIFY MeshFeatureProcessor thread-safety].
- Physics actor add/remove [VERIFY AzPhysics scene write threading].
- EBus notifications.
- State-table writes.
- Global-state reads for re-evaluation.

### Text state diagram

```text
                    want (dist ≤ R_in, or pinned)
   ┌──────────┐ ───────────────────────────────▶ ┌────────┐
   │ Unloaded │                                   │ Queued │──cancel (dist > R_out & !pinned)──▶ Unloaded
   └──────────┘ ◀──────────────┐                  └───┬────┘
        ▲                      │                      │ dequeued (I/O slots + bytes-in-flight available)
        │                 ┌────┴─────┐            ┌───▼───────┐
        │                 │ Unloading│◀──cancel───│ IOPending │──timeout/err (retry ≤2, backoff 0.5 s, 2 s)──▶ Failed
        │                 └────▲─────┘            └───┬───────┘
        │                      │                      │ all requests complete
        │                      │                  ┌───▼──────────┐
        │                      ├──────cancel──────│ Deserializing│──validation err──▶ Failed
        │                      │                  └───┬──────────┘
        │                      │                      │ job done (completion queue → main)
        │   evict (LRU / dist  │                  ┌───▼───────────┐
        └──── > R_evict) ──────┴──────────────────│ LoadedInactive│◀──────────────┐
                                                  └───┬───────────┘               │
                                                      │ want active (dist ≤ R_in)  │ done
                                                  ┌───▼──────────┐          ┌─────┴───────┐
                                                  │ Instantiating│─abort*──▶│ Deactivating│
                                                  └───┬──────────┘          └─────▲───────┘
                                                      │ all slices done           │ dist > R_out & !pinned
                                                  ┌───▼────┐                      │ (+ min dwell 3 s)
                                                  │ Active │──────────────────────┘
                                                  └────────┘
   Failed ──(player-forced retry after 10 s, or leaves R_out)──▶ Unloading
   * abort = finish the current slice, then reverse already-created objects in Deactivating
```

### Transition rules (invariants)
1. Only the main-thread scheduler writes state. Workers report results through the completion queue.
2. `Active` is reachable only from `Instantiating`. `Instantiating` only from `LoadedInactive`.
3. A pinned layer (mission or airlock pin) never leaves `Active` or cancels while pinned.
4. Each **load cycle** increments the chunk-layer generation, which invalidates all old `ChunkHandle`s (§3.5).
5. Layer dependency: a chunk's **G layer must be `Active` before its I or D layers instantiate**. S is independent of G (visual can lead or trail). H1 shows whenever S isn't `Active`.

### Hysteresis [T]

| Layer | R_in | R_out | R_evict (drop LoadedInactive) | Min dwell |
|---|---|---|---|---|
| G | 256 m | 320 m | 512 m | 3 s |
| S | 384 m | 448 m | 640 m | 3 s |
| D | 128 m | 192 m | 320 m | 2 s |
| I (seamless interior) | 48 m from portal / inside | 64 m | 128 m | 2 s |
| H1 | 2 000 m | 2 200 m | 2 400 m | 5 s |

- Distances are measured from the player (or the camera for S/D/H1 if farther ahead) to the chunk's 2D AABB.
- A 64 m band plus minimum dwell stops thrash when circling a boundary. The worst case is one transition per 3 s per layer.
- The **LoadedInactive cache** is LRU, capped at 256 MB (§3.5). It evicts at R_evict or under pressure.

### Priority function

Per wanted chunk-layer, compute an **ETA to contact** and sort ascending:

```text
d        = distance(player, chunkAABB)                      // meters
v_along  = max(0, dot(velocity, dirToChunk))                // m/s
eta_lin  = d / max(v_along, v_walk = 1.5)                    // seconds
eta_road = pathDistanceAlongRoadPrediction / max(speed, v_walk)
           // only if the chunk intersects the predicted road path:
           // horizon 3 s, GPS route if set, else lane-follow with
           // branch weight 1/n at intersections
eta      = min(eta_lin, eta_road)
key      = (pinClass, eta × layerFactor)
           // pinClass: 0 = mission/airlock pin, 1 = normal
           // layerFactor: G 1.0, I 1.0, S 1.5, D 3.0, H1 4.0
```

- Look-ahead radius growth along the velocity direction: R_in_eff = R_in + v·3 s·max(0, cosθ). That is +165 m forward at 55 m/s. It never shrinks behind the player.
- Scheduler cost: a linear SoA scan of the ≤ 1 024 descriptors plus a partial sort of ≤ 128 candidates. ≤ 0.2 ms [T].

### Cancellation
- `Queued`: remove from the queue.
- `IOPending`: cancel streamer requests where supported [VERIFY `AZ::IO::IStreamer` cancel]. Otherwise drop asset refs and discard results on completion.
- `Deserializing`: set the atomic cancel flag. The job checks it between sections and releases its arena.
- `Instantiating`: abort at the slice boundary → `Deactivating`.
- Trigger: distance > **R_out** (not R_in), so minor jitter doesn't cancel useful work.

### Failure / timeout

| Stage | Timeout [T] | Action |
|---|---|---|
| IOPending | 5 s | Retry ×2 (backoff 0.5 s, 2 s) → `Failed` |
| Deserializing | 1 s | Validation failure → `Failed` immediately (data bug, no retry) |
| Instantiating | 10 s total | Log + continue (slow, not failed). Telemetry flag |
| `Failed` G layer | — | **Enable the coarse collision proxy** from H1. Log error with chunk id and stage. HUD warning in non-release builds. Retry only after the player leaves R_out or 10 s passes |

---

## 3.4 Async Asset and I/O Pipeline

### Packaging

| Data | Format | Built by | Loaded via |
|---|---|---|---|
| Gameplay/interactive content (G, I layers, cells) | `.prefab` source → `.spawnable` product | O3DE prefab builder [VERIFY] | AssetManager async load → `SpawnableEntitiesInterface` spawn with an `EntitySpawnTicket` [VERIFY names] |
| Bulk static + detail data (S, D layers) | Custom **ChunkData** product (`.ltchunk`): SoA instance arrays (mesh id, material id, transform 3×4 float chunk-local), physics primitive lists, variant tables, stable-id tables | Custom AssetBuilder (AssetBuilderSDK) from generator/editor-exported JSON [VERIFY builder registration API] | Custom `AZ::Data::AssetHandler` [VERIFY] → deserialized into the chunk arena |
| Meshes, materials, textures | Standard Atom products | Asset Processor | Atom (mesh asset loads, streaming images) |
| Cooked collision | PhysX mesh assets [VERIFY product type] | Asset Processor | AssetManager |
| Release packaging | **One .pak per district + one global .pak**, files ordered by chunk coordinate for locality | AssetBundler with per-district seed lists [VERIFY AssetBundler/seed-list workflow in 26.05] | Archive system, transparent to AssetManager [VERIFY] |

**Bulk entity-less path (decision):**
- Static kit geometry and decorative props are rendered by acquiring mesh handles directly from the Atom mesh feature processor, with no entity per instance [VERIFY public API usable from a gem].
- Their collision is added as static actors through the AzPhysics scene interface [VERIFY].
- Interactive objects stay entities.
- *Why:* entity activation is the dominant main-thread cost (§3.2).
- *Fallback if the API proves unusable:* prefab path only, with entity budgets cut 40% (and a re-derived radius).
- *Rejected:* entities for everything, which caps density at ~600 objects per urban chunk.

### Scheduling model (per frame, main thread)

1. **Update desired set:** SoA scan computes `want` per chunk-layer using radius and hysteresis (§3.3).
2. **Prioritise:** ETA keys, partial sort.
3. **Issue I/O:** dequeue while `inFlightLayers < 4` and `bytesInFlight + estBytes ≤ 64 MB` [T]. Requests carry a **deadline = now + ETA − T_ready_est** and a priority class (below) [VERIFY streamer deadline/priority support].
4. **Dispatch deserialization:** ≤ 2 concurrent jobs [T] via AZ jobs [VERIFY current job/task API in 26.05: `AZ::Job` vs `AZ::TaskGraph`].
5. **Drain the completion queue** (mutex-guarded, ≤ tens of items per second).
6. **Instantiate:** process `Instantiating` work items in ETA order until **1.5 ms** elapsed (checked per item; items are pre-split into ≤ 0.25 ms units, e.g. 10 entities or 250 bulk instances).
7. **Deactivate:** same pattern, 0.3 ms.
8. **Publish** stats to the budget tracker and HUD (atomics, relaxed).

### I/O priority classes (highest first)
1. Pinned / G layer collision + gameplay
2. VO and dialogue audio (small, latency-sensitive: in-car dialogue V39)
3. S layer
4. Texture mips (Atom streaming)
5. D layer
6. H1 prefetch

### Back-pressure and graceful degradation

**Deficit signal:** for any wanted G-layer chunk, `slack = ETA − T_ready_est`. `T_ready_est` = queue position × measured mean per-chunk times (EMA).

| Level | Trigger [T] | Action | Visible effect |
|---|---|---|---|
| L0 | slack ≥ 1.5 s for all G | Normal | — |
| L1 | Any G slack < 1.5 s | Suspend D and H1 issue. S drops to class 5. Instantiation slice grows to `min(4 ms, 16.6 − lastFrameMsExclStreaming)` | Fewer small props briefly; no hitch |
| L2 | Any G slack < 0.75 s | Enable the H1 **coarse collision proxy** for G chunks not yet `Active`. S chunks keep showing H1 | Distant buildings show proxy detail longer |
| L3 | Any G slack < 0.3 s | **Vehicle speed soft-cap** (AD-06 hook): `v_cap = max(15 m/s, (d_unready − 30 m) / T_ready_est)`, applied as a smooth drive-force limiter, not braking | Car feels "governed" for a moment |
| L4 | Player within 16 m of a G chunk with neither `Active` nor proxy | Freeze simulation + fade to black until ready (target: never). **Logged as a defect** | Visible stall |

**Fade-in:** S layers appear behind H1 at ≥ 384 m. Popping is minimised by distance, hysteresis and LOD matching (§3.7). No custom dither shader in v1 unless Atom provides one [VERIFY].

---

## 3.5 C++ Memory Management Strategy

### Allocator architecture

| Allocator | Source | Used for |
|---|---|---|
| `AZ::SystemAllocator` | O3DE [VERIFY current allocator set after the 2023 rework] | Default for engine objects and for our long-lived singletons via `AZ_CLASS_ALLOCATOR` [VERIFY macro] |
| `AZ::PoolAllocator` / `AZ::ThreadPoolAllocator` | O3DE [VERIFY existence and thread-safety semantics] | Small fixed-size objects allocated by engine glue (bus handlers, small records) |
| AZStd containers + allocator param | O3DE | All gem containers. Default allocator only in cold paths |
| **ChunkBlockPool** (custom) | Ours | **192 MB reserved at boot** = 768 × 256 KB blocks [T]. One `VirtualAlloc`-style reservation, uniform blocks → **zero external fragmentation** |
| **ChunkArena** (custom, linear) | Ours | Per chunk-layer bump allocator over blocks from ChunkBlockPool. Holds all deserialized ChunkData. Freed wholesale (blocks returned) at unload |
| **FrameScratch** (custom, linear, per thread) | Ours | Main 1 MB, each worker 256 KB. Reset per frame/job. Candidate lists, sort buffers |
| **Fixed pools** (custom) | Ours | ChunkDescriptor table (fixed array of 1 024), handle slot map (fixed 2 048 slots), work items (fixed 512) |
| **I/O staging ring** (custom) | Ours | 8 × 8 MB = 64 MB compressed-read staging + 32 MB decompression scratch |

Rule: **engine-owned objects** (entities, components, render handles, physics actors, asset data) are *not* in our arenas. They are owned through tickets/handles, counted per chunk, and released in `Deactivating`/`Unloading`.

### Memory budgets (min spec) [T]

**RAM, 6 144 MB ceiling:**

| Category | MB | Derivation |
|---|---|---|
| Engine/runtime baseline (modules, Atom CPU side, PhysX SDK, AZ systems) | 1 500 | ASSUMPTION, deliberately high. **Measured in P2 week 1** on an empty-level launcher |
| Texture staging / CPU-side residency | 256 | Atom streaming upload staging |
| Mesh CPU-side buffers | 256 | Asset buffers before upload, plus CPU-readable meshes |
| Collision (G layers + proxies) | 256 | ~36 G chunks (worst) × ≤ 6 MB + 40 MB proxies |
| Audio | 192 | 128 MB resident SFX + 64 MB streaming buffers (music, VO) |
| Animation | 256 | ASSUMPTION: cast + peds motion data (P3 refines) |
| Entities/components | 192 | 36 × 600 prefab entities + ~300 dynamic ≈ 22 k entities × ~6 KB avg (+ margin) |
| Chunk arenas (ChunkBlockPool) | 192 | ~116 resident chunk-layers (36 G + 64 S + 16 D) × 1.65 MB avg |
| LoadedInactive cache | 256 | LRU warm cache of staged data (asset refs + arenas beyond the pool) |
| Streaming I/O buffers | 96 | Staging ring 64 + decompression 32 |
| Global resident world data | 128 | Road graph, heightmap 5.3, H2 + skyline, descriptors, water, TOD presets |
| Navigation (reserved, P3) | 128 | — |
| AI/traffic (reserved, P3) | 128 | — |
| UI | 96 | — |
| Game/global state + mission/dialogue data | 64 | Save state < 1 MB + tables |
| VFX | 96 | — |
| Frame scratch + job temp | 32 | 1 + 6 × 0.25 MB per frame, double-buffered, plus temp |
| **Sum** | **4 124** | |
| **Headroom** | **2 020 (33%)** | Reduce to ≤ 15% after P2/P3 measurement |

**VRAM, 4 096 MB ceiling:**

| Category | MB | Derivation |
|---|---|---|
| Render targets + swapchain + shadow maps (1080p) | 512 | HDR RGBA16F 16.6 + depth 8 + TAA history 33 + post chain ~60 + directional cascades 4 × 2048² × 4 B = 64 + local shadow atlas 4096² × 4 B = 64 + transient pool ~250 [VERIFY Atom pass attachment sizes via its memory view] |
| Texture streaming pool | 1 600 | ~40 district trim sets (2K BC7/BC5, 3 maps ≈ 13 MB) = 520 + ~150 prop sets (1K ≈ 3.3 MB) = 500 + hero chars/vehicles 260 + terrain/decals 100 + HLOD atlases 100 = 1 480 full-res, plus margin. Mip streaming trims distant sets [VERIFY Atom streaming image pool budget control] |
| Mesh vertex/index | 400 | 64 S chunks × 2 MB unique + kits 150 + props 60 + chars/vehicles 60 |
| Instance/culling buffers | 64 | ~30 k instances × ~100 B + culling data |
| Reflection probes | 96 | 2 per chunk × ~43 S chunks × 0.52 MB (256² cube BC6H with mips) = 45 MB + global [VERIFY Atom probe format/compression] |
| VFX | 64 | — |
| UI | 64 | — |
| Water/sky/weather | 64 | — |
| Driver/Atom pools, descriptor heaps, fragmentation | 300 | ASSUMPTION |
| **Sum** | **3 164** | |
| **Headroom** | **932 (23%)** | — |

### Ownership and lifetime rules

1. **StreamingSystem owns** the ChunkTable (descriptors + runtime records). Each **ChunkLayerRecord owns** its arena, spawn tickets, bulk render handles and physics actor handles. Nothing else owns chunk content.
2. **External references use `ChunkHandle`** = {ChunkId (u16 grid index), layer (u8), generation (u16)}. A handle resolves only if the record's current generation matches, so stale handles resolve to null. There is no raw `ChunkLayerRecord*` outside the gem.
3. **Entities are referenced by `AZ::EntityId`** and re-resolved every use. `AZ::Entity*` and component pointers are never cached across frames.
4. **Persistent and cross-chunk references use `StableId`** = {ChunkId, local authored u32}. Never pointers. Never EntityIds, because spawn regenerates them [VERIFY spawnable entity id regeneration].
5. **RAII:**
   - `ChunkArena` returns its blocks in its destructor and asserts `bytesOutstanding == 0` for tracked sub-objects.
   - `ScopedSpawnTicket`, `ScopedMeshHandle` and `ScopedPhysicsActor` release on destruction and assert on the main thread.
   - No naked `new`/`delete` in the gem. `AZStd::unique_ptr` with AZ allocators for heap ownership.
6. **Cross-chunk content is forbidden** in authored data. An object straddling a border belongs to the chunk that contains its pivot. The validator checks that its bounds don't exceed the chunk AABB by more than 16 m [T].

### Hot-path rules
- **Zero heap allocations** in per-frame scheduler, priority and instantiation-slice code. Enforced in debug by an allocation-counter guard around `StreamingSystem::Tick` (assert if > 0).
- Containers are **pre-reserved at boot** to grid capacity. `AZStd::fixed_vector` where a bound is known.
- **SoA descriptor table** (each array ≤ 4 KB for 1 024 chunks): `coordX[] coordY[] (int16)`, `layerState[L][] (uint8)`, `generation[L][] (uint16)`, `etaMs[] (float)`, `flags[] (uint16: pinned, failed, interiorHost…)`, `budgetClass[] (uint8)`.
- **False sharing:** each job writes only its own `alignas(64)` output record. Atomic stat counters are padded to 64 B. Workers never write the SoA table.
- No virtual dispatch in the inner scan. Policies (`ILodPolicy`, `ICullingPolicy`, priority) are evaluated per candidate after the scan.

### Fragmentation strategy (multi-hour sessions)
1. All chunk data goes through the uniform ChunkBlockPool. Reuse is exact, so there is no fragmentation by construction.
2. The bulk path eliminates most per-object engine allocations for static content.
3. Engine-side variable allocations (entities) are bounded by the per-chunk entity caps. Churn is limited by hysteresis and dwell.
4. GPU: a fixed-size texture streaming pool. Mesh buffers come from Atom pools [VERIFY Atom buffer pool sub-allocation].
5. **Verification:** 2 h soak, memory slope < 1 MB/min (MVS D5), plus SystemAllocator fragmentation stats if exposed [VERIFY].

### Leak and corruption detection
- **Per-chunk tagging:** every arena allocation carries a ChunkId/layer tag (debug/profile).
- ChunkBudgetTracker counters per chunk-layer: entities spawned/despawned (ticket callbacks), mesh handles acquired/released, physics actors added/removed, asset refs held.
- **Unload verification:** at `Unloading → Unloaded`, assert all counters are back to zero and `blocksAcquired == blocksReturned`. Profile builds log and continue. Release builds keep counters only.
- **ASan:** the `win-vs2022-asan` preset runs core tests, gem tests and a 10-minute testbed route weekly. Custom pools call ASan poison/unpoison on block return [VERIFY O3DE allocator compatibility under ASan].
- **O3DE allocation records:** enabled in profile soak runs to catch leaks into the SystemAllocator [VERIFY settings key/CVar for allocation records in 26.05].
- **Canary:** in debug, arena blocks carry head/tail guard words checked on return.

### Threading model

| Data / operation | Writer | Readers | Synchronisation |
|---|---|---|---|
| SoA descriptor + state table | Main only | Main; HUD via snapshot | None (single writer) |
| Job input (chunk id, file blobs, global-state snapshot for variants) | Main (before dispatch) | Worker | Immutable after dispatch; happens-before via job dispatch |
| Chunk arena during `Deserializing` | Worker (exclusive) | — | Ownership transfer at dispatch and completion |
| Completion queue | Workers / I/O callbacks | Main | `AZStd::mutex`, short critical section (≤ 100/s) |
| Cancel flags | Main | Worker | `std::atomic<bool>` (acquire/release) |
| Stats counters | Any | HUD | Relaxed atomics, 64 B padded |
| Engine spawn / render handles / physics add-remove | **Main only** (unless [VERIFY] proves thread-safe) | — | — |
| EBus requests/notifications | Main only | — | Buses declared single-threaded. Calls from other threads are a contract violation (asserted) |
| Global gameplay state | Main | Main; workers via snapshot | Snapshot copy |

---

## 3.6 Culling Pipeline (cheapest first)

| Layer | Provided by | We build | Savings class | Failure modes → mitigation |
|---|---|---|---|---|
| (a) Chunk streaming cull | — | **Yes**: non-resident = zero cost; layer radii | Very high (everything beyond radius) | Pop-in at speed → ETA priority, look-ahead, H1 swap at ≥ 384 m |
| (b) Frustum cull | **Atom** visibility system (octree / IVisibilityScene) [VERIFY] | Correct bounds for bulk instances | High (~60–75% of resident objects) | Wrong bounds → validator checks mesh bounds vs. instance transforms |
| (c) Occlusion | **Atom Occlusion Culling Plane** components, software-rasterized (Masked Occlusion Culling) [VERIFY] | **Authored occluders baked into kit pieces**: 1–2 planes inside each building module's walls. ≤ 32 active occluders/frame, chosen by projected area [T] | High in dense urban (Old Mercy streets, Glass Mile edge), low in rural | Over-cull at speed (stale occlusion) → occluders are static and conservative (inset 0.5 m). Missing occluder → budget report flags urban chunks with < N occluders |
| (d) Small-object / screen-size cull | Atom mesh LOD min screen coverage [VERIFY] + D-layer radius | Per-class thresholds (§3.7) enforced by the validator | Medium (props/detail) | Visible popping of small props → D layer fades by radius, not screen size, inside 128 m |
| (e) Interior/exterior cells | — | **Yes**: per-portal rectangle test (in frustum ∧ within 64 m ∧ facing). Interior render group visible only if the camera is inside the cell or a portal passes. In airlock interiors, exterior S layers beyond G are hidden and H1 shows through windows | High inside interiors | Visible interior through a window not on the portal list → every exterior opening is a portal (validator) |
| (f) Shadow-caster cull | Atom per-cascade culling [VERIFY] | Caster flags: detail props never cast. Props < 1 m stop casting beyond 40 m. H1 only in the last cascade, H2 never. Local shadowed lights ≤ 2 per chunk, ≤ 8 global | High (shadow draws ≈ 50% of main) | Shadow popping at the cascade edge → caster LOD = render LOD + 1, hysteresis |

**Occlusion decision (dense urban):** authored occluders on kit pieces + engine software rasterization.
- *Trade-off:* the occluder authoring cost is paid once per kit piece, not per placement.
- *Rejected:* GPU Hi-Z (custom render pipeline/shader work, out of scope); full software rasterization of render meshes (CPU cost); precomputed PVS (incompatible with streaming and variants).

---

## 3.7 LOD Strategy

Screen size = projected bounding-sphere diameter / screen height. Thresholds are switch-down points; switch-up = threshold × 1.1 (hysteresis). All values [T].

| Class | LOD0 tris (max) | LOD1 | LOD2 | LOD3 | Switch thresholds (LOD0→1 / 1→2 / 2→3 / cull) | Shadow LOD | Beyond LOD3 |
|---|---|---|---|---|---|---|---|
| Building kit piece | 5 k | 50% | 25% | 10% | 0.25 / 0.10 / 0.04 / none | Render LOD + 1 | H1 (chunk swap) |
| Unique building (≤ 12/chunk) | 40 k | 50% | 25% | 10% | 0.30 / 0.12 / 0.05 / none | +1 | H1 |
| Prop (standard) | 3 k (hero 8 k) | 40% | 15% | — | 0.15 / 0.05 / — / 0.005 | +1; none if < 1 m beyond 40 m | Culled |
| Vehicle (player/hero) | 80 k | 50% | 20% | 5% (box) | 0.40 / 0.15 / 0.05 / 0.004 | +1 | Culled |
| Vehicle (traffic) | 40 k | 50% | 20% | 5% | 0.35 / 0.12 / 0.04 / 0.004 | +1 | Culled |
| Character (reference only; P3) | 50 k hero / 20 k ped | 50% | 20% | 8% | 0.50 / 0.20 / 0.08 / 0.004 | +1 | Culled |
| Terrain tile (128 m) | 32 k (1 m grid) | 8 k (2 m) | 2 k (4 m) | 512 (8 m) | **By chunk ring, not screen size**: ≤ 128 / 256 / 384 m / H1 | LOD2 | H1 (16 m grid) |
| Tree | 15 k | 5 k | 1.5 k | cross-card impostor (≤ 16 tris) | 0.30 / 0.12 / 0.05 / 0.01 | LOD2; none for LOD3 | Culled / H1 canopy blob |
| Grass/ground cover | 200 per clump | — | — | — | D-layer radius only (≤ 40 m) | Never | — |

**HLOD:**
- **T1 (per chunk):** one merged proxy mesh ≤ 4 k tris + 512² BC7 atlas + coarse collision (≤ 500 tris, roads + building boxes). Shown when the chunk's S layer isn't `Active` and the chunk's cluster is within 1 000 m. The swap is exactly 1:1 per chunk, with no partial overlap.
- **T2 (4×4 cluster, 512 m):** ≤ 20 k tris + 1K atlas. Shown when the cluster's nearest point is > 1 000 m. Always resident (~30 clusters ≈ 0.6 M tris, ~100 MB including atlases). **Skyline (AD-13)** belongs to the same tier.
- **Draw-call check:** T1 annulus 384–1 000 m ≈ π(1000² − 384²)/128² ≈ 164 chunks → ≤ 164 draws. T2 ≤ 40 draws.
- **Generation:** offline, a Blender Python script (P4): merge → decimate → bake atlas → export `HL_` FBX + coarse `_COL`. Rejected: hand-authored HLODs (solo capacity); third-party simplifiers (cost/licensing).

**Material/texture LOD:**
- LOD2+ use a `_Far` material instance (no detail normal, no parallax). H1/H2 use a single atlas material.
- Mip streaming within the 1 600 MB pool; priority by screen size [VERIFY Atom mip-bias/priority controls].

**Cross-fade:**
- No custom dithering shader in Phase 1 scope. If Atom provides dithered LOD transitions, enable them [VERIFY]. Otherwise rely on hysteresis bands plus switching distances where the pop is ≤ ~2 px.

**Authoring pipeline (Blender → O3DE):**
1. One FBX per `SM_`. Mesh nodes `<Name>_LOD0…_LOD3` and `<Name>_COL` [VERIFY O3DE scene builder LOD suffix convention (`_lod1` style) and case sensitivity; rename the exporter output to match if needed].
2. Units: 1 unit = 1 m, +Z up, transforms applied, triangulated on export.
3. An exporter script (P4) writes a sidecar manifest `<Name>.ltmeta` (JSON): per-LOD tri counts, material slots, bounds, collision type and size, asset class.
4. The `.assetinfo` is committed with the LOD and physics mesh groups configured [VERIFY `.assetinfo` scene-manifest schema].

**Validation (`tools/validate_lods.py`, spec):** for each `SM_`/`SK_`/`HL_`:
- `.ltmeta` exists.
- LOD count matches the class.
- Each LOD ≤ 60% of the previous one's tris.
- LOD0 ≤ the class cap.
- `_COL` exists if the class is collidable; primitives or ≤ 1 hull of ≤ 32 verts for props.
- Material slots ≤ class cap.
- Bounds within the chunk-border tolerance for world pieces.

Output and exit codes as for the naming validator (03 §2.4).

---

## 3.8 Map Optimization Rules (per chunk, by district profile) [T]

**Frame-level derivation:**
- Main-view draw budget 3 000 = ~12 visible full-detail chunks after frustum+occlusion × 200 + H1 164 + H2 40 + dynamic (vehicles, characters, VFX) ~300 + margin.
- Visible-triangle budget ≈ 6 M main view (min-spec GPU, leaving room for shadows at ~50%).

| Rule | Dense urban | Industrial | Rural | Water | Interior cell |
|---|---|---|---|---|---|
| Draw calls (S+G+D at LOD0 distance) | ≤ 250 | ≤ 180 | ≤ 120 | ≤ 30 | ≤ 200 |
| Triangles, sum of LOD0 | ≤ 500 k | ≤ 400 k | ≤ 400 k (veg) | ≤ 50 k | ≤ 300 k |
| Unique materials | ≤ 48 (district palette ≤ 120) | ≤ 32 | ≤ 24 | ≤ 8 | ≤ 40 |
| Unique texture sets outside district trims | ≤ 4 | ≤ 4 | ≤ 2 | ≤ 1 | ≤ 6 |
| Prefab entities (G) | ≤ 600 | ≤ 400 | ≤ 200 | ≤ 50 | ≤ 400 |
| Bulk instances (S+D) | ≤ 4 000 | ≤ 3 000 | ≤ 6 000 | ≤ 200 | ≤ 1 500 |
| Unique (non-kit) meshes | ≤ 12 | ≤ 8 | ≤ 8 | ≤ 2 | ≤ 20 |
| Local lights: unshadowed / shadowed | ≤ 24 / ≤ 2 | ≤ 16 / ≤ 2 | ≤ 8 / ≤ 1 | ≤ 4 / 0 | ≤ 16 / ≤ 2 |
| Static collision | ≤ 20 k tris mesh + ≤ 200 primitives | same | ≤ 10 k + 100 | ≤ 1 k | ≤ 15 k + 150 |
| Occluders | ≥ 8 (kit-provided) | ≥ 4 | 0 | 0 | portals instead |
| Audio emitters / reverb zones | ≤ 16 / ≤ 4 | ≤ 16 / ≤ 2 | ≤ 12 / ≤ 1 | ≤ 4 / 0 | ≤ 12 / ≤ 4 |
| Disk payload, G / S / D (compressed) | ≤ 6 / 8 / 2 MB | same | ≤ 4 / 8 / 3 MB | ≤ 1 / 2 / 0 MB | ≤ 6 / 8 / 2 MB |
| Arena (deserialized) per layer | ≤ 4 MB | ≤ 4 MB | ≤ 4 MB | ≤ 1 MB | ≤ 4 MB |
| Variant sites (AD-09) | ≤ 3 | ≤ 3 | ≤ 2 | 0 | ≤ 3 |

**Rules:**
1. **Instancing:** any mesh+material pair used ≥ 4× in a chunk **must** use the bulk path. Atom instances identical mesh/material draws [VERIFY GPU instancing behaviour/requirements in 26.05].
2. **Modular reuse:** ≥ 70% of building façade area comes from kit pieces.
3. **Trim sheets:** buildings use district trim sheets (2K BC/N/ORM). No texture > 2K without a waiver. The hero 4K waivers are Tyler, Lena, Mack, Rafe, the casino vessel interior, and Mack's car.
4. **Lighting policy:** no lightmaps (TOD states plus a dynamic sun make baked lightmaps a 3× data multiplier).
   - Lighting = dynamic sun/moon + per-chunk baked reflection probes per lighting state + emissive materials for neon/windows + capped dynamic lights.
   - Storm lightning = one global unshadowed directional flash.
   - Night vehicles: **only the player's headlights cast shadows**. AI headlights are unshadowed, max 12 lit at once by distance.
   - Atom diffuse GI probe grids are **not used** by default; evaluate in P4 only [VERIFY cost].
5. **Collision seams:** road and terrain collision vertices on chunk edges are generated identically by the tool on both sides (welded). Dynamic objects never use triangle-mesh colliders.
6. **Waivers:** stored in `DT_ChunkWaivers.json` with reason and expiry phase. Known waivers: casino vessel (materials ×2), evacuation event (entities ×1.5 while the event is active), Mack's workshop interior (lights ×1.5).

**Budget report tool (`tools/chunk_budget_report.py`, spec):**
- **Inputs:** chunk/cell prefabs (JSON), `.ltmeta` manifests, the ChunkData builder's stats JSON (emitted per `.ltchunk`), `DT_ChunkWaivers.json`, district profile table (this section, mirrored in `DT_ChunkBudgets.json`).
- **Per chunk:**
  - estimated draws: unique (mesh, material, LOD0) pairs; bulk instance groups count once
  - LOD0 tris; unique materials and texture sets with estimated VRAM
  - lights (shadowed/unshadowed); collision tris/primitives
  - prefab entities, bulk instances, occluders
  - disk bytes per layer, arena estimate, variant sites
- **Output:**
  - CSV + Markdown summary (worst 20 chunks, per-district totals, % of budget)
  - exit code 1 if any chunk is over budget without a valid waiver
- **When it runs:** pre-push when `Assets/World/` changes (03 §2.5). The in-engine HUD (05) cross-checks runtime numbers against the report. A > 15% divergence is a tool bug.

---

## 3.9 Day/Night, Weather and Crisis Hooks (streaming/memory only)

**Always resident (global):**
- TOD lighting state + sun/sky/fog parameters (4 presets × < 1 KB).
- Weather state and intensity.
- **Global flood level.**
- Camera-attached rain particle system.
- Global wetness parameter.
- Weather audio beds.
- Lightning controller.
- Global water planes.
- Global heightmap (5.3 MB).

**Per chunk:**
- **Reflection probes for the current lighting state only.** There are 3 baked probe sets: Day, Twilight, Night. Storm reuses Night plus a global tint, so there's no 4th set.
- TOD transitions happen only at fades or mission boundaries (AD-07). The new state's probe set for ~43 S chunks ≈ 45 MB loads during the fade (≤ 0.3 s at 150 MB/s).
- Disk cost: 3 × ~0.5 MB × 2 probes ≈ 3 MB per chunk.

**Act III flooding: recommendation = global flood-level variable (AD-08):**
1. One camera-centred water surface at height `floodLevel`. A disc of radius 2 km, moved with the camera in 128 m steps to avoid swimming. Terrain and buildings above the level hide it naturally through depth, so there is no per-chunk water geometry.
2. Material hook: a global per-frame constant (`floodLevel`, `wetness`) that materials *may* read for darkening below the waterline. **Shader implementation is P4 scope.** Only the constant's existence is reserved here.
3. Gameplay depth = `floodLevel − groundHeight`: from a physics raycast when the G layer is `Active`, otherwise from the global heightmap.
4. Act III world changes (closures, barricades, closed businesses, emergency spawns) = AD-09 variant groups re-evaluated **in place**, plus road-graph edge closures. **No chunk reloads.**

*Rejected:* swapping per-chunk flooded variants (doubles S/G data, reload hitches during the pursuit in M14); simulated flow (out of scope).

---

## 3.10 Save/Load Interaction

**Boundary:** a save = **global state sections** + **chunk delta store**. Live entity state is never serialized. Saves happen only at safehouses/checkpoints with heat = 0, so no in-flight combat or vehicle physics needs saving. Mission state is saved as checkpoint IDs (01 §1.2).

| Scope | Content | Persisted when |
|---|---|---|
| Global | All 01 §1.2 global rows | Save |
| Chunk-local | DeltaRecord {StableLocalId u32, kind u8 (door, breakable, container, movedVehicle, pickup), value u16, optional transform (chunk-local float3 + yaw)} | Recorded into the in-RAM delta store when the event happens. Flushed from `Active` layers on save. Persists across unload |

Per-chunk delta cap: 256 records [T]. When the cap is hit, the oldest *cosmetic* kinds (breakables) are evicted first. Gameplay kinds (containers, pickups) are never evicted.

**Format and versioning:**
- File = `Header {magic 'LTSV', fileVersion u16, gameVersion string, timestamp u64, payloadCRC32}` + TLV sections `{sectionId u32, sectionVersion u16, size u32, payload}`.
- Each section owns a monotonic `sectionVersion` and a chain of migration functions vN→vN+1. Unknown sections are skipped (forward compatible). Missing sections take defaults.
- The chunk delta section stores `ChunkContentHash u32` per chunk. On mismatch (content patched), deltas whose StableLocalId no longer exists are dropped and logged.
- **Custom binary TLV with hand-written versioned readers/writers.** This decouples the save format from engine reflection changes across O3DE upgrades. *Trade-off:* more code. *Rejected:* SerializeContext/JSON reflection for saves (engine-churn coupling) [VERIFY versioning support anyway for tooling export].

**Interface (declarations only; full headers in Part 4):**

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
        virtual bool Read(class SaveReader& reader, uint16_t storedVersion) = 0; // migrates if storedVersion < current
        virtual void ResetToDefaults() = 0;                                       // section absent in file
    };
}

namespace LowTide::Streaming
{
    struct StableId;          // {ChunkId chunk; uint32_t local;}  (Part 4 ChunkTypes.h)
    struct ChunkDeltaRecord;  // {uint32_t local; uint8_t kind; uint16_t value; optional transform}

    // Owned by StreamingSystem; persists across chunk unload. Main thread only.
    class IChunkDeltaStore
    {
    public:
        virtual ~IChunkDeltaStore() = default;
        virtual void Record(const StableId& id, const ChunkDeltaRecord& record) = 0; // overwrite by id
        virtual size_t GetDeltas(uint16_t chunkId, const ChunkDeltaRecord*& outRecords) const = 0;
        virtual void ClearChunk(uint16_t chunkId) = 0;
    };
}
```
