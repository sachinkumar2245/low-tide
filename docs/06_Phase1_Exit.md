# FILE: /docs/06_Phase1_Exit.md

**Project:** LOW TIDE | **Phase:** 1 exit | **Doc status:** v0.2: [VERIFY] index triaged against the O3DE **26.10.0** source (tag `2610.0`, commit `b6122815`, 2026-11-21)

---

## 4.4 Risk Register (top 10, streaming foundation)

L = likelihood, I = impact (H/M/L). Updated after the source triage.

| # | Risk | L | I | Early-warning signal | Mitigation | Retired in |
|---|---|---|---|---|---|---|
| R1 | **O3DE API churn / behaviour differs from docs**, incl. regressions in the brand-new 26.10.0 | M (was H: 77 claims now confirmed or corrected from source) | H | A Week-4 spike contradicts §4.6. Engine-glue compile breaks | Pin `2610.0`; take `2610.1` only at a gate. Engine code isolated in `Source/Engine`, Core std-only. The engine-facing headers already compile against AzCore 26.10 | P2 |
| R2 | **Spawn cost ≫ 25 µs/entity or mesh-handle cost ≫ 20 µs/handle** invalidates radii and caps | M | H | E1 > 40 µs/entity; E3 acquire > 40 µs or `Simulate` > 1 ms per 10 k handles | Merged sections (fewer handles); smaller G prefabs; larger R_in (04 §3.2 formula); fewer D props | P2 week 4–5 |
| R3 | **Entity-less paths hit undocumented limits**: no locks in MeshFeatureProcessor; `CreateShape` thread-safety unknown; cross-batch spawn references | M (API existence now confirmed ✔ V-51, V-68) | H | Concurrency-checker asserts; physics asserts off-main; broken references in E1 | Main-thread-only calls at tick 78. Move `CreateShape` to main if needed. One spawnable per layer section if cross-batch refs fail. Final fallback: prefab path with −40% entities | P2 week 4–5 |
| R4 | **Main-thread saturation.** Atom has no render thread, PhysX `FinishSimulation` blocks main, and spawn processing is main-only and not self-slicing (✔ V-37, V-44, V-49). Slack is only 1.8 ms | **H** | H | Empty-level baseline (task 8) shows RPI + physics > 6.5 ms. Streaming p99 > 2 ms | Spawn batches ≤ 10. Merged sections. Measure the RPI/physics split in task 8 and re-budget game/AI. Reduce draws/handles. PhysX substep tuning | P2 exit |
| R5 | **No min-spec hardware for validation** | H | M | No min-spec run by the P2 exit | Used Ryzen 5 3600 + GTX 1660 Super + SATA SSD box. Stricter dev-machine thresholds | P3 exit |
| R6 | **Asset pipeline throughput (solo):** merged sections, LODs, HLODs, occluder planes, probes | H | H | OM authoring < 0.1 km²/week in P4. Section merge/bake > 10 min per chunk | Builder automates section merge + LOD regeneration. Blender export + `.ltmeta`. Validators. Contingency: cut Briar to 1.5 km² | P4 exit |
| R7 | **Scope creep** | H | H | Non-Keep feature started. Estimates slip > 15% | Equal-cost cut logged in 02 for any addition | Never; every gate |
| R8 | **Long-session memory growth / fragmentation.** The engine exposes **no fragmentation stats** (◐ V-40); RHI pools don't defragment (◐ V-64) | M | H | S1/S2 slope > 1 MB/min. `reserved − used` trend rising | Uniform block pool. Bounded handles. Named child allocator + allocation records in soak runs | P3 |
| R9 | **Editor / AP scaling** with ~300 chunks × layers | M | M | AP full rebuild > 2 h; context level load > 60 s | District context levels; prefab caps; testbed full-grid AP run in P2 | P3/P4 |
| R10 | **I/O < 150 MB/s effective**; **AssetManager has no load cancel** (◐ V-36) | M | M | HUD < 100 MB/s; wasted loads after cancel > 20% | Pak ordering by chunk. Aggregate blobs. Larger R_in. Drop refs on cancel. Evaluate direct `IStreamer` reads with `RescheduleRequest`/`Cancel` for `.ltchunk` (✔ V-35) | P2 (loose) / P5 (paks) |

---

## 4.5 Phase 1 Exit Gate (binary)

| # | Item | Status |
|---|---|---|
| G1 | GDD verb list complete and reconciled with the story bible (01 v0.2) | ☑ |
| G2 | Systems matrix, set-piece risks, content hooks (01) | ☑ |
| G3 | Scope cut: map −60.0%, features −62.7% (02) | ☑ |
| G4 | **MVS signed off by owner** | ☐ owner |
| G5 | Owner decisions: adult-content policy for v1.0; Briar contingency trigger | ☐ owner (engine version decided: 26.10.0, 2026-11-21) |
| G6 | Engine `2610.0` built from source; Editor + Asset Processor launch | ☐ Windows |
| G7 | Repo initialised; attributes/ignore committed; hooksPath set | ☑ 2026-11-21: public repo `sachinkumar2245/low-tide` |
| G8 | LFS verified by clean clone | ☑ 2026-11-21 (macOS). Re-run on Windows after `git lfs install` |
| G9 | O3DE project + `LowTideStreaming` gem created; templates copied | ☐ Windows |
| G10 | **Interface headers compile in an empty gem** (MSVC, profile) | ◐ All 11 headers syntax-check under clang. **The 2 AzCore headers compile against the real 26.10 AzCore** with static asserts on tick orders and bus policies (05 §4.2). MSVC gem build pending |
| G11 | Validator + budget-report specs | ☑ (updated for verified naming: `_Phys`, last-segment texture suffix, no `_ORM`) |
| G12 | Owner cold-read ≥ 48 h after writing | ☐ owner (04 v0.2 changed substantially, so read that) |
| G13 | Testbed spec + test plan (05) | ☑ (v0.2: spawn batching, mesh-handle tests) |
| G14 | Risk register | ☑ (updated) |
| G15 | **[VERIFY] index triaged** | ◐ Source triage done (§4.6): **46 confirmed, 31 corrected (docs updated), 15 open** (each with a P2 task or phase). Owner skim of the corrected rows pending |
| G16 | `v0.1.0` tag pushed; NAS mirror | ☐ |

### Phase 2 entry requirements
1. G1–G16 ☑.
2. All open **B** rows in §4.6 have run their Week-4/5 task.
3. `tests/core_standalone` builds and runs.
4. Engine baseline measured (task 8). 04 §3.0 main-thread split and §3.5 RAM baseline updated from measurements.
5. No conflict between 01–06 (G12).

### First 10 tasks, Week 4 (in order)

| # | Task | Output | Closes |
|---|---|---|---|
| 1 | Windows setup: CMake 4.2.3, clone `2610.0`, `get_python.bat`, build engine, `register --this-engine`, `create-project`, `create-gem`, `enable-gem`, copy templates, **profile build with all headers** | G6, G9, G10 ☑ | V-03, V-92 |
| 2 | **Spawn spike (E1):** `SpawnEntities` batches of 10 queued at tick 78; cost at TICK_GAME; high vs. default lane; **cross-batch reference test** | ADR-001, µs/entity | V-89, R2, R3 |
| 3 | **Mesh-handle spike (E3):** 1 k/10 k/24 k static handles, `r_meshInstancingEnabled=true`, ray tracing off; `Simulate` ms; LOD switch distances per decay rate | ADR-002 | V-91, R2, R3 |
| 4 | **Entity-less physics spike (E4):** `CreateShape` on a worker, `AddSimulatedBodies` at tick 78, raycast checks | ADR-003 | V-88, R3 |
| 5 | Texture import check: one test texture per suffix (`_BC _N _Rough _Metal _AO _E _H _O _Mask`) → confirm presets/formats | Import report | V-22 (runtime) |
| 6 | Core implementation + tests: ChunkTypes, ChunkState, arena/pool, BudgetTracker (U1–U4, U11–U14), standalone + AzTest | Green tests | — |
| 7 | `validate_names.py` (incl. `PresetsByFileMask` cross-check, `_Phys`) + pre-commit hook | Hook active | — |
| 8 | **Engine baseline:** empty level, profile launcher. RAM (`QueryMemInfo`, `GetAllocatorStats`), VRAM (RHI stats), main-thread split RPI / physics / tick | 04 §3.0 + §3.5 updated | V-49 (budget), R4 |
| 9 | `generate_testbed.py` v0 + `editor_build_prefabs.py` (`SaveTemplateToString` + file write) for a 2×2 Z1 block + `.ltchunk` builder/handler stub | 4 chunks in the editor | V-29, V-30, V-31, V-34 |
| 10 | HUD v0 (`#if !defined(_RELEASE)`) + `lt_benchRoute` CVar route playback + CSV logger | First R1 dry run | V-47 |

**Week 5 spike:**
- `.ltchunk` builder + handler complete
- `AssetLoadParameters` deadline/priority (V-87)
- occluder-plane creation from gem code (V-90)
- `LowTideStreamingAllocator` child allocator + allocation-record config (V-40)

---

## 4.6 [VERIFY] Index (triaged)

**Status:**
- ✔ = confirmed as written
- ◐ = partly right; the corrected fact is in the docs
- ✖ = wrong; the docs are changed
- ○ = open (source can't settle it; resolved by the named task or phase)

**B** = blocks P2 week 4–5. Evidence paths are relative to the engine repo at `2610.0`.

| ID | Claim (as originally assumed) | Status | Verified fact / resolution | Evidence | B |
|---|---|---|---|---|---|
| **Engine / build** | | | | | |
| V-01 | Latest stable tag | ✔ | `2610.0` released 2026-11-20 (not a pre-release). **Locked 26.10.0** | GitHub release metadata | — |
| V-02 | CLI syntax | ✔ | `register --this-engine`; `create-project -pp [-pn] [-tn]`; `create-gem -gp`; `enable-gem -gp/-gn -pp` | `scripts/o3de/o3de/register.py:929`, `engine_template.py:2834`, `enable_gem.py:174` | — |
| V-03 | VS2022 17.14 supported; VS2026? | ◐ | Presets only vs2022/vs2019; min toolset v142. VS2026 is only a CMake-version warning, so treat it as unsupported | `cmake/Platform/Common/CMakePresets.json:123`, `MSVC/Configurations_msvc.cmake:41` | ○ task 1 |
| V-04 | Bundled CMake ≥ 3.25, presets v6 | ✖ | Floor 3.24. Presets **v4**. Source checkouts bundle nothing. Install CMake **4.2.3**. Template fixed | `CMakeLists.txt:10`, `cmake/Packaging.cmake:42` | — |
| V-05 | C++ standard | ✔ | C++20 (required) | `cmake/Configurations.cmake:173` | — |
| V-06 | Bundled Python | ◐ | 3.10.13 via `get_python.bat` into `%USERPROFILE%\.o3de\Python`; `python.cmd` is a wrapper | `cmake/3rdParty/Platform/Windows/Python_windows.cmake:12` | — |
| V-07 | Game gem in `Gem/` | ✔ | — | `Templates/DefaultProject/Template/Gem/CMakeLists.txt` | — |
| V-08 | Gem file/target names | ✖ | `.API`, `.Private.Object`, module, `.Editor*`, `.Tests`; `_api/_private/_shared/_tests` file lists | `Templates/DefaultGem/Template/Code/CMakeLists.txt:28-248` | — |
| V-09 | `LY_UNITY_BUILD`, `LY_MONOLITHIC_GAME` | ✔ | Unity ON by default. Monolithic disables tools/tests | `cmake/LYWrappers.cmake:9`, `cmake/Monolithic.cmake:9` | — |
| V-10 | Target names | ✔ | `<Project>.GameLauncher`, `AssetProcessor`, `AssetProcessorBatch`, `Editor` | `Code/LauncherUnified/launcher_generator.cmake:170` | — |
| V-11 | Test labels | ✔ | `SUITE_<suite>`; name `<NAME>.<suite>::TEST_RUN` | `cmake/LYTestWrappers.cmake:21,114` | — |
| V-12 | Config defines | ✔ | See 03 §2.5. Release strips asserts/traces | `cmake/Platform/Common/Configurations_common.cmake:41` | — |
| V-13 | AP batch flags | ✔ | `--platforms`, `--project-path`, … | `AssetProcessor/native/utilities/assetUtils.cpp:686` | — |
| V-14 | `_savebackup` folder | ◐ | Inferred from AP excludes (`_savebackup`, `_autobackup`); both added to the ignore list later if seen | `Registry/AssetProcessorPlatformConfig.setreg:173` | — |
| V-15 | PhysX5 gem | ◐ | `PhysX5` (+Debug). **PhysX 4 removed** | `Gems/PhysX/Core/PhysX5/gem.json:2` | — |
| V-16 | ASan | ✔ | `LY_BUILD_WITH_ADDRESS_SANITIZER` | `MSVC/Configurations_msvc.cmake:173` | — |
| V-17 | GitHub LFS pricing | ○ | External: check GitHub billing | — | — |
| V-18 | Newer Blender LTS | ○ | External: check at setup | — | — |
| **Assets / pipeline** | | | | | |
| V-19 | `assets_source/` not scanned | ◐ | Not scanned by default; a project `ScanFolder` entry could add it (forbidden) | `AssetProcessorPlatformConfig.setreg:130` | — |
| V-20 | LOD/collision node naming | ✔ | `_lod1`…`_lod5`, `_phys`, case-insensitive. **Our naming changed to `_LOD1…`, `_Phys`** | `Gems/SceneProcessing/Registry/SoftNameSettings.setreg:5` | — |
| V-21 | `.assetinfo` is JSON | ✔ | — | `SceneCore/Containers/SceneManifest.cpp:150` | — |
| V-22 | Texture suffix mapping | ◐ | Last `_` segment → `PresetsByFileMask` in `ImageBuilder.settings`; project override file `Config/AtomImageBuilder/ImageBuilder.settings`. **`_ORM` unmapped and unsupported by StandardPBR (`.r` only)**, so separate maps | `ImageBuilder.settings:46-205`; `RoughnessInput.azsli:49` | ○ task 5 |
| V-23 | `parentMaterial` | ✔ | — | `RPI.Edit/Material/MaterialSourceData.cpp:51` | — |
| V-24 | `.physxmaterial` | ✔ | — | `PhysXEditorMaterialAsset.h:28` | — |
| V-25 | Particle asset type | ○ | Gem is `OpenParticleSystem`; asset type unchecked (P4) | `engine.json:60` | — |
| V-26 | Sequence asset | ◐ | No standalone asset: `SequenceComponent` inside a prefab | `Gems/Maestro/Code/Source/Components/SequenceComponent.cpp:120` | — |
| V-27 | Prefab → spawnable builder | ✔ | — | `Gems/Prefab/PrefabBuilder/PrefabBuilderComponent.cpp:54` | — |
| V-28 | Stable editor ids; spawn regenerates ids | ✔ | Alias-path hash; `MakeId` per spawn | `InstanceEntityIdMapper.cpp:49`; `SpawnableEntitiesManager.cpp:351` | — |
| V-29 | Editor prefab scaling | ○ | Empirical (R9) | — | task 9 |
| V-30 | Custom AssetBuilder | ✔ | `AssetBuilderDesc` + `RegisterBuilderInformation` | `AssetBuilderSDK.h:242` | — |
| V-31 | Custom AssetHandler | ✔ | `AssetManager::RegisterHandler`; `GenericAssetHandler<T>` | `AssetManager.h:172` | — |
| V-32 | Cooked mesh product | ✔ | `.pxmesh` | `Pipeline/MeshAssetHandler.cpp:25` | — |
| V-33 | Bundles / paks | ✔ | AssetBundler commands; `OpenPacks("@products@/*.pak")` | `AssetBundler/source/utils/utils.cpp:50`; `GameApplication.cpp:55` | — |
| V-34 | Python prefab authoring | ◐ | Create entity / add components / `CreatePrefabInMemory` / `SaveTemplateToString`; **no direct file save** | `PrefabPublicRequestHandler.cpp:30`; `PrefabSystemComponent.cpp:92` | — |
| **Runtime: core / streaming** | | | | | |
| V-35 | Streamer deadline/priority/cancel | ✔ | Deadline µs, priority u8, `RescheduleRequest`, best-effort `Cancel`; callback on executor thread | `AzCore/IO/IStreamer.h:65-304` | — |
| V-36 | AssetManager async threads | ◐ | Load in `LoadAssetJob`; **ready events on main via `DispatchEvents`**; no load cancel | `AssetManager.cpp:127,1827` | — |
| V-37 | Spawnable API & threading | ✔ | Main thread at TICK_GAME; **auto-activate; not sliced; no cancel of queued spawns**; two priority lanes. Design changed to batched `SpawnEntities` | `SpawnableEntitiesInterface.h:32-430`; `SpawnableSystemComponent.cpp:52` | — |
| V-38 | Job API | ◐ | Both `AZ::Job` (AssetManager, PhysX) and `AZ::TaskGraph` (newer) live; ours uses TaskGraph | `AzCore/Task/TaskGraph.h:34` | — |
| V-39 | Allocators | ✔ | `AZ_CLASS_ALLOCATOR`, `AZ_CHILD_ALLOCATOR_WITH_NAME`; PoolAllocator not thread-safe | `Memory/ChildAllocatorSchema.h:20`; `PoolAllocator.h:197` | — |
| V-40 | Allocation records / fragmentation | ◐ | `allocator_tracking_<Name>=<Mode>` via startup cfg; `sys_DumpAllocators`; **no fragmentation stats** | `AllocatorManager.cpp:244`; `IAllocator.h:214` | ○ wk 5 |
| V-41 | Memory stats API | ✔ | `GetAllocatorStats`; `QueryMemInfo` | `AllocatorManager.h:108`; `Process/ProcessInfo.h:21` | — |
| V-42 | EBus traits; `AZ::Interface` | ✔ | Verified and compile-tested | `EBus/Policies.h:29,63`; `Interface/Interface.h:24` | — |
| V-43 | Component macros | ✔ | `AZ_COMPONENT_DECL/IMPL` (template) or `AZ_COMPONENT`; compile-tested | `Component/Component.h:382-504` | — |
| V-44 | TickBus + physics placement | ✔ | INPUT 75, GAME 80, PHYSICS_SYSTEM 200 (static-asserted); PhysX blocks main | `TickBus.h:28`; `PhysX/.../SystemComponent.cpp:443` | — |
| V-45 | Include paths | ✔ | Compile-tested | — | — |
| V-46 | `AZ_TYPE_INFO_SPECIALIZE` | ◐ | Exists; must be placed in the type's namespace or `AZ` | `RTTI/TypeInfoSimple.h:64` | — |
| V-47 | CVars from launcher command line | ◐ | `AZ_CVAR`; `--name=value` executed after gems load; `--console-command-file` | `ComponentApplication.cpp:1001`; `Launcher.cpp:601` | — |
| **Rendering (Atom)** | | | | | |
| V-48 | DX12 feature level | ✔ | `D3D_FEATURE_LEVEL_12_0` | `RHI/DX12/.../SystemComponent_Windows.cpp:56` | — |
| V-49 | Render threading | ◐ | **No render thread**: RPI ticks on main, jobs record. Budget rebuilt | `RHISystem.cpp:283`; `BootstrapSystemComponent.cpp:874` | ○ task 8 |
| V-50 | Float world, no camera-relative | ✔ | — | `TransformServiceFeatureProcessor.h:57`; `ViewSrg.azsli:15` | — |
| V-51 | Entity-less mesh API | ✔ | **GO with caveats**: main-thread only, no locks, per-handle cost, ray tracing default true. Design changed (merged sections, ≤ 24 k handles) | `MeshFeatureProcessorInterface.h:154-302` | — |
| V-52 | Automatic instancing | ◐ | **Off by default** (`r_meshInstancingEnabled`); shared materials required; disables `SetDrawItemEnabled` | `MeshFeatureProcessor.cpp:2077` | — |
| V-53 | Frustum culling via octree | ✔ | Octree ±16 384 m | `RPI.Public/Culling.cpp:135`; `OctreeSystemComponent.cpp:16` | — |
| V-54 | Occlusion planes + MOC | ◐ | Planes only occlude; MOC fixed 1080p; no cap/CVar | `Culling.cpp:608` | — |
| V-55 | Per-LOD screen thresholds | ◐ | **Per-mesh** `LodConfiguration` (coverage + decay). Design changed | `RPI.Public/Culling.h:81` | — |
| V-56 | Shadow LOD / exclusion | ◐ | No shadow LOD; exclusion via material `castShadows`. Design changed (`_NoShadow` materials) | `DirectionalLightFeatureProcessor.cpp:1158` | — |
| V-57 | Light culling limits | ◐ | Tiled; 256 lights per tile max | `CoreLights/LightCullingConstants.h` | — |
| V-58 | Dithered LOD cross-fade | ✖ | Not supported; popping accepted | `Culling.cpp:980` | — |
| V-59 | Streaming pool budget; force-resident mips | ◐ | `r_streamingImagePoolBudgetMb`, mip bias, `SetTargetMip`/`SetStreamingPriority`; **no hard pin** | `StreamingImagePool.h:65`; `StreamingImage.h:94` | — |
| V-60 | Probe format BC6H | ✖ | **R9G9B9E5**. Budget recomputed (128² chunk probes) | `IBLSpecular.preset:13` | — |
| V-61 | DiffuseProbeGrid | ✔ | Real-time needs ray tracing; not used | `DiffuseProbeGridFeatureProcessorInterface.h:25` | — |
| V-62 | RHI memory stats | ✔ | `RHIMemoryStatisticsInterface`; ImGui GPU memory view | `RHIMemoryStatisticsInterface.h` | — |
| V-63 | Draw/tri stats | ✔ | `GetFrameStatistics`; pipeline-statistics queries | `PassSystemInterface.h:56` | — |
| V-64 | Buffer pool sub-allocation | ◐ | Sub-allocates; **no defragmentation** | `DX12/.../BufferMemoryAllocator.h:44` | — |
| V-65 | Video playback | ○ | `VideoPlaybackFramework` gem listed, not checked; AD-14 avoids it anyway | `engine.json` | — |
| V-66 | Render-to-texture | ◐ | `RenderToTexturePass`, `CreateRenderPipelineForImage` (deferred feature) | `RenderToTexturePass.h` | — |
| V-67 | Ocean water gem | ○ | Not checked; AD-02 avoids it | — | — |
| **Physics** | | | | | |
| V-68 | Entity-less static bodies | ◐ | `AddSimulatedBodies` + `StaticRigidBodyConfiguration`; **main thread, outside the physics tick** | `PhysicsScene.h:71-321`; `PhysXScene.h:136` | — |
| V-69 | Sleep threshold | ✔ | `m_sleepMinEnergy = 0.005` (≈ 0.1 m/s) | `RigidBodyConfiguration.h:54` | — |
| V-70 | Origin shift not exposed | ✔ | Not exposed anywhere | grep: no hits | — |
| V-71 | No double-precision build | ✔ | Float only | `Math/Vector3.h:342` | — |
| V-72 | CCD config | ✔ | `m_ccdEnabled` per body + scene `m_enableCcd` (default false) | `RigidBodyConfiguration.h:61`; `SceneConfiguration.h:40` | — |
| V-73 | Character controller | ✔ | `PhysX::CharacterControllerComponent` | `CharacterControllerComponent.h:32` | — |
| V-74 | Raycast API | ✔ | `RayCastRequest` + `QueryScene` (+ batch/async) | `PhysicsSceneQueries.h:163` | — |
| V-75 | Vehicle gem | ○ | Not in checkout; P2 gameplay decides (custom raycast vehicle) | — | P2 gameplay |
| **Other gems** | | | | | |
| V-76 | Profiler macros | ✔ | `AZ_PROFILE_SCOPE(budget, …)` + `AZ_DECLARE/DEFINE_BUDGET`; pluggable via `AZ::Interface<Profiler>`; no Tracy | `Debug/Profiler.h:25`; `Debug/Budget.h:53` | — |
| V-77 | ImGui in launcher | ◐ | Yes; **not compiled out in release** (CMake bug), so we guard the HUD ourselves | `Gems/ImGui/Code/CMakeLists.txt:8,108` | — |
| V-78 | Terrain gem streaming (rejected path) | ◐ | Spawners register at Activate, so runtime-spawnable; box shapes; clipmaps are material-only. Revisit P2 wk 6 | `TerrainLayerSpawnerComponent.cpp:100` | — |
| V-79 | Recast tile streaming | ◐ | No public per-tile API; raw `dtNavMesh` `addTile/removeTile` via `GetNavigationObject()` | `RecastNavigationMeshBus.h:26` | P3 |
| V-80 | EMotionFX root motion / events | ✔ | `MotionExtractionRequestBus`; `OnMotionEvent` | `Integration/MotionExtractionBus.h:21` | — |
| V-81 | UI stack | ✔ | LyShine | `Gems/LyShine/gem.json` | — |
| V-82 | Spline component | ✔ | `LmbrCentral::SplineComponent` | `Shape/SplineComponent.h:56` | — |
| V-83 | Light-level sampling (stealth) | ○ | P3 | — | P3 |
| V-84 | Runtime material properties | ◐ | `Material::SetPropertyValue` + `Compile`; shared vs. unique instances affect instancing | `RPI.Public/Material/Material.h:65` | — |
| V-85 | SerializeContext versioning | ✔ | `Version(n, VersionConverter)` | `Serialization/SerializeContext.h:151` | — |
| V-86 | Steam adult-content policy | ○ | External, before P5 | — | — |
| **New from triage** | | | | | |
| V-87 | `AssetLoadParameters` carries deadline/priority for our loads | ○ | Week 5 spike | `AzCore/Asset/AssetCommon.h` (likely) | B |
| V-88 | `Physics::SystemRequests::CreateShape` is safe off the main thread | ○ | Task 4 (E4) | `AzFramework/Physics/SystemBus.h` | B |
| V-89 | Entity references stay valid across multiple `SpawnEntities` calls on one ticket | ○ | Task 2 (E1) | `SpawnableEntitiesManager.cpp` | B |
| V-90 | Occluder planes can be created/enabled from gem code without entities | ○ | Week 5 spike (`OcclusionCullingPlaneFeatureProcessorInterface`) | `Feature/Common/.../OcclusionCullingPlane/` | B |
| V-91 | Exact per-LOD coverage derivation from `m_qualityDecayRate` | ○ | Task 3 (E3) records switch distances | `MeshFeatureProcessor.cpp:2686` | — |
| V-92 | `gem.json` dependency names for our gem | ○ | Task 1 | generated `gem.json` | B |

**Triage totals:**
- **46 ✔ confirmed**
- **31 corrected (27 ◐ + 4 ✖) with docs updated:** 03 stack/naming/build, 04 v0.2, 05, headers, CMake presets
- **15 open (○)**:
  - 5 new P2-blocking spikes: V-87–V-90, V-92
  - 2 Week-4 runtime checks: V-03, V-22
  - the rest are external or later-phase

The design changes these forced are listed at the top of 04 v0.2.
