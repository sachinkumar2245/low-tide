# FILE: /docs/06_Phase1_Exit.md

**Project:** LOW TIDE | **Phase:** 1 exit | **Doc status:** v0.1

---

## 4.4 Risk Register (top 10, streaming foundation)

L = likelihood, I = impact (H/M/L).

| # | Risk | L | I | Early-warning signal | Mitigation | Retired in |
|---|---|---|---|---|---|---|
| R1 | **O3DE API churn / undocumented behaviour** (spawnables, prefab API, allocators, EBus details): the 26.05 surface differs from the [VERIFY] assumptions | H | H | Any P2-blocking [VERIFY] item refuted. Engine-glue compile breaks | Pin 26.05.x with no mid-phase upgrades. Engine code stays in `Source/Engine`; logic in std-only Core. Week-4 spikes on blocking items first. Engine patches tracked as `.patch` | P2 (blocking items); re-opened at each engine upgrade |
| R2 | **Entity activation cost ≫ 25 µs** invalidates radii, entity caps and slice budget | M | H | E1 > 40 µs/entity | Bulk path for static content. Lower per-chunk entity caps. Larger R_in (04 §3.2 formula). Smaller work items | P2 week 4–5 |
| R3 | **Bulk entity-less render/physics path not usable from a gem** (no public API, or not thread/lifetime-safe) | M | H | Spike: can't acquire mesh handles or add static actors outside entities | Fallback: prefab path only, entity caps −40%, radii re-derived. Same `ILodPolicy`/HUD contracts | P2 week 4–5 |
| R4 | **Main-thread-only engine operations exceed the 1.5 ms slice** (spawn, physics add, render handle create are not divisible) | M | H | P3 metric: streaming ms p99 > 2 ms; spikes at specific work items | Pre-split work items (≤ 10 entities). Spread per-chunk activation across frames. Investigate engine spawn priority/async options [VERIFY]. Raise the slice into frame slack (L1) | P2 exit (R1, R2 routes) |
| R5 | **No min-spec hardware for validation**, so budgets are only validated on the dev machine | H | M | No min-spec run logged by the P2 exit | Buy/borrow a used Ryzen 5 3600 + GTX 1660 Super + SATA SSD box (or downgrade parts in a spare case). Dev-machine thresholds are set stricter (P1: 12 ms) | P3 exit |
| R6 | **Asset pipeline throughput (solo)**: LODs, HLODs, collision and probes per chunk exceed the authoring rate in 02 §2.1 | H | H | Old Mercy authoring in P4 measures < 0.1 km²/week. HLOD/probe bake > 10 min per chunk | Automate: Blender export + `.ltmeta`, HLOD generator, validators, budget report. Kit reuse ≥ 70%. Contingency: cut Briar to 1.5 km² (02) | P4 exit |
| R7 | **Scope creep** (the bible is rich; features return quietly) | H | H | A feature or system started that isn't Keep in 02 §2.3. Week estimates slip > 15% | Change control: any addition needs an equal-cost cut logged in 02 with the point total. Phase gates re-check the 62.7% cut | Never retired; checked at every gate |
| R8 | **Long-session memory growth / fragmentation** in engine-owned allocations | M | H | S1/S2 slope > 1 MB/min. LeakReports. SystemAllocator growth | Uniform block pool. Bulk path. Per-layer counters. Allocation records in soak runs [VERIFY]. Bounded LoadedInactive cache | P3 (2 h soak on full slice) |
| R9 | **Editor / Asset Processor scaling** with ~300 chunks × 5 layers + cells (AP rebuild time, editor load of context levels, prefab size) | M | M | Full AP rebuild > 2 h. `L_Author_OM` load > 60 s. Prefab edit lag | District context levels. Entity caps per prefab (1 000). Testbed full-grid AP run in P2 as an early probe. AP job tuning [VERIFY] | P3 (testbed) / P4 (real content) |
| R10 | **Effective I/O throughput < 150 MB/s** on SATA (small files, pak decompression, AV scanners) | M | M | HUD measured MB/s < 100 under R1 | Aggregate the per-layer files into one blob per chunk-layer. Order the pak by chunk coordinate. Choose a cheaper compression. Increase R_in (budget allows up to ×3.7 slower) | P2 (loose files) / P5 (paks) |

---

## 4.5 Phase 1 Exit Gate (binary)

| # | Item | Status at doc time |
|---|---|---|
| G1 | GDD verb list complete (64 verbs) and reconciled against the story bible (01 v0.2) | ☑ written |
| G2 | Systems requirement matrix + set-piece risks + content-sensitivity hooks complete (01) | ☑ written |
| G3 | Scope cut documented: map −60.0%, features −62.7% (02) | ☑ written |
| G4 | **MVS signed off by owner** (Old Mercy, 3 vehicles, Prologue + M2, criteria D1–D11) | ☐ owner |
| G5 | Owner decisions recorded: adult-content policy for v1.0; Briar contingency trigger | ☐ owner |
| G6 | Engine 26.05.x built from source at the pinned tag; Editor + Asset Processor launch | ☐ |
| G7 | Repo initialised. `.gitattributes`/`.gitignore` committed. `core.hooksPath` set | ☑ 2026-11-21: public repo `sachinkumar2245/low-tide`, `main` pushed |
| G8 | **LFS verified by cloning to a clean directory** (03 §2.3 procedure; SHA-256 match, `git lfs fsck` OK) | ☑ 2026-11-21 (macOS): probe PNG SHA-256 match, `git lfs fsck` OK. Re-run on the Windows dev machine after `git lfs install` there |
| G9 | O3DE project `LowTide` + gem `LowTideStreaming` created; headers copied from `tools/templates` | ☐ |
| G10 | **Interface headers compile in an empty gem** (MSVC, profile build, one TU including all 11 headers) | ☐ (std-only headers pass clang C++17/20 `-Werror` syntax check) |
| G11 | Naming validator spec written (03 §2.4); LOD validator + budget report specs written (04 §3.7, §3.8) | ☑ written |
| G12 | Streaming design reviewed: owner cold-read ≥ 48 h after writing, every derivation re-checked, questions logged as ADRs | ☐ owner |
| G13 | Testbed spec + test plan complete (05) | ☑ written |
| G14 | Risk register complete (§4.4) | ☑ written |
| G15 | **[VERIFY] index triaged**: every row has a status (Confirmed / Refuted → design change / Deferred to a named P2 spike). All **B** (P2-blocking) rows have a week-4/5 task | ☐ owner |
| G16 | `v0.1.0` annotated tag pushed; NAS mirror updated | ☐ |

### Phase 2 entry requirements
1. G1–G16 all ☑.
2. All **B** [VERIFY] items are either confirmed or have a scheduled spike in tasks 1–10 below.
3. The standalone core test project (`tests/core_standalone`) builds and runs (even with zero tests).
4. The engine baseline is measured: empty-level launcher RAM/VRAM/frame time, written into 04 §3.5 in place of the 1 500 MB assumption.
5. No unresolved conflict between 01–05 (cross-reference check done during G12).

### First 10 tasks, Week 4 (in order)

| # | Task | Output | Retires |
|---|---|---|---|
| 1 | Run 03 §2.3 + §2.5: git init, LFS verify, create project + gem, copy headers, compile one TU with all headers in the `profile` build | G7–G10 ☑ | R1 (partial) |
| 2 | **Spike: spawnable spawn/despawn** via ticket in the profile launcher; confirm threading, activation-on-spawn, entity id regeneration | ADR-001 | V-27, V-28, V-37 |
| 3 | **E1 entity activation microbenchmark** (N = 100…2 000) | µs/entity figure; 04 §3.2 updated | R2 |
| 4 | **Spike: bulk path**: acquire/release Atom mesh handles + static PhysX actors from gem code, 4 000 instances | ADR-002 (go / fallback) | R3 |
| 5 | Texture suffix → image-preset mapping (`.setreg`); import test for `_BC/_N/_ORM` | Correct presets on import | V-22 |
| 6 | Core implementation + tests: ChunkTypes, ChunkState, ChunkMemoryArena/BlockPool, BudgetTracker (U1–U4, U11–U14), standalone + AzTest | Green tests | — |
| 7 | `validate_names.py` + pre-commit hook | Hook active | — |
| 8 | Engine baseline measurement (empty level): RAM, VRAM, frame time, launcher startup | 04 §3.5 RAM table updated | Entry req. 4 |
| 9 | `generate_testbed.py` v0 (neutral JSON + `.ltchunk` source) + `editor_build_prefabs.py` spike for one 2×2 Z1 block | 4 generated chunks in the editor | V-95 |
| 10 | Streaming HUD v0 (states, ms, memory counters) + kinematic route playback + CSV logger skeleton | First R1 dry run on 4 chunks | — |

---

## 4.6 [VERIFY] Index

Columns:
- **ID**
- **Claim** (what I assumed)
- **Sec** = where it is used
- **Confirm in**: likely O3DE source/doc location; paths are approximate and relative to the engine repo root
- **B** = blocks P2 week 4–5

| ID | Claim | Sec | Confirm in | B |
|---|---|---|---|---|
| **Engine / build** | | | | |
| V-01 | 26.05.x latest patch tag | 03 §2.1 | GitHub o3de/o3de releases; docs.o3de.org release notes | B |
| V-02 | `o3de.bat register --this-engine`, `create-project --project-path … --template-name DefaultProject`, `create-gem`, `enable-gem` syntax | 03 §2.5 | `scripts/o3de.bat`, `scripts/o3de/o3de/*.py`; docs "Project/Gem CLI" | B |
| V-03 | VS2022 17.14 supported (VS2026 status) | 03 §2.1 | docs.o3de.org/docs/welcome-guide/requirements | B |
| V-04 | Bundled CMake ≥ 3.25 (presets v6) | 03 §2.1, §2.5 | 26.05 release notes; `cmake/` | B |
| V-05 | Engine C++ standard (17 vs 20) | 03 §2.1 | `cmake/Platform/Common/Configurations_common.cmake` / `CMAKE_CXX_STANDARD` | — |
| V-06 | Bundled Python path/version | 03 §2.1 | `python/python.cmd`, `python/get_python.*` | — |
| V-07 | Project template places the game gem in `Gem/` | 03 §2.2 | `Templates/DefaultProject/` | — |
| V-08 | Gem template file names (`*_files.cmake`, `*_api_files.cmake`, `*_shared_files.cmake`) and target names (`.Static`, `.Tests`, `.Editor`) | 03 §2.4, 05 §4.2 | `Templates/DefaultGem/` (or the CppTool gem template); `cmake/LYWrappers.cmake` (`ly_add_target`) | B |
| V-09 | `LY_UNITY_BUILD`, `LY_MONOLITHIC_GAME` cache vars | 03 §2.5 | `cmake/` (search the var names); docs "Build monolithic" | — |
| V-10 | Target names `Editor`, `<Project>.GameLauncher`, `AssetProcessorBatch` | 03 §2.5 | `Code/Editor`, `Code/LauncherUnified`, `Code/Tools/AssetProcessor` CMakeLists | B |
| V-11 | ctest naming/labels (`SUITE_main` etc.) | 03 §2.5 | `cmake/LyAutoGen`/`LYTestWrappers.cmake` (`ly_add_googletest`) | — |
| V-12 | debug/profile/release define sets | 03 §2.5 | `cmake/Platform/Common/Configurations_common.cmake` | — |
| V-13 | `AssetProcessorBatch.exe` flags | 03 §2.5 | `Code/Tools/AssetProcessor`; docs "Asset Processor batch" | — |
| V-14 | Editor save-backup folder name (`_savebackup`) | 03 .gitignore | Run the editor once; inspect the project tree | — |
| V-15 | `PhysX5` gem name and default in 26.05 | 03 §2.1 | 26.05 release notes; `Gems/PhysX/` | — |
| V-16 | ASan works with O3DE allocators | 03 §2.1, 04 §3.5 | Try the `asan` preset; search `AZ_ENABLE_ADDRESS_SANITIZER`-like options in `cmake/` | — |
| V-17 | GitHub LFS pricing/quota | 03 §2.1 | GitHub billing docs | — |
| V-18 | Newer Blender LTS than 4.5 | 03 §2.1 | blender.org/download/lts | — |
| **Assets / pipeline** | | | | |
| V-19 | Asset Processor default scan folders exclude `assets_source/` outside the project | 03 §2.2 | `Registry/AssetProcessorPlatformConfig.setreg` | — |
| V-20 | Scene-builder LOD node suffix (`_lod1`…) and physics mesh naming, case sensitivity | 03 §2.4, 04 §3.7 | `Gems/SceneProcessing`, `Code/Tools/SceneAPI` (LOD rule); docs "FBX settings / LODs" | — |
| V-21 | `.assetinfo` scene-manifest schema | 04 §3.7 | Open a generated `.assetinfo`; `Code/Tools/SceneAPI/SceneCore/Containers/SceneManifest` | — |
| V-22 | Image builder suffix → preset mapping config location/keys | 03 §2.4 | `Gems/Atom/Asset/ImageProcessingAtom/` (preset `.preset` files, builder settings `.setreg`) | B |
| V-23 | Atom material parent inheritance (`parentMaterial`) | 03 §2.4 | `Gems/Atom/RPI/Code/Source/RPI.Edit/Material/MaterialSourceData*` | — |
| V-24 | `.physxmaterial` extension | 03 §2.4 | `Gems/PhysX/Core/Code/Source/` material asset | — |
| V-25 | 26.05 particle asset type (new native particle system) | 03 §2.4 | 26.05 release notes; new particle gem | — |
| V-26 | Sequence/cutscene asset type (Track View successor?) | 01 V57, 03 §2.4 | `Gems/Maestro/`; docs "Track View" | — |
| V-27 | Prefab builder `.prefab` → `.spawnable`; prefab JSON schema stability | 04 §3.4, 05 §4.1 | `Code/Framework/AzToolsFramework/AzToolsFramework/Prefab/`, `Gems/Prefab/PrefabBuilder` | B |
| V-28 | Stable prefab entity ids across edits; spawn regenerates EntityIds | 03 §2.4, 04 §3.5 | `AzToolsFramework/Prefab/Instance/`; `AzFramework/Spawnable/` | B |
| V-29 | Editor prefab focus-mode / nested editing workflow; editor scaling with large prefabs | 04 §3.2 | docs "Prefabs → Focus Mode"; empirical (R9) | — |
| V-30 | AssetBuilderSDK custom builder registration (`.ltchunk`) | 04 §3.4 | `Code/Tools/AssetProcessor/AssetBuilderSDK/AssetBuilderSDK.h`; sample builders in `Gems/` | B |
| V-31 | Custom `AZ::Data::AssetHandler` for runtime asset type | 04 §3.4 | `Code/Framework/AzCore/AzCore/Asset/AssetManager.h`, `AssetTypeInfoBus.h` | B |
| V-32 | PhysX cooked mesh product type | 04 §3.4 | `Gems/PhysX/Core/Code/Source/Pipeline/` | — |
| V-33 | AssetBundler seed lists / per-district paks; archive transparent to AssetManager | 04 §3.4 | `Code/Tools/AssetBundler/`; `Code/Framework/AzFramework/AzFramework/Archive/` | — |
| V-34 | Editor Python bindings (`azlmbr`) can create entities and save prefabs in batch | 05 §4.1 | `Gems/EditorPythonBindings/`; docs "Editor Python scripting" | B |
| **Runtime: core / streaming** | | | | |
| V-35 | AZ::IO Streamer: read deadlines, priorities, cancel | 04 §3.3, §3.4 | `Code/Framework/AzCore/AzCore/IO/Streamer/` (`IStreamer.h`, `IStreamerTypes.h`) | B |
| V-36 | AssetManager async load threads | 04 §3.3 | `AzCore/Asset/AssetManager.cpp` | — |
| V-37 | `SpawnableEntitiesInterface`, `EntitySpawnTicket`, spawn/despawn names, main-thread processing, activation on spawn | 04 §3.3, §3.4 | `Code/Framework/AzFramework/AzFramework/Spawnable/SpawnableEntitiesInterface.h`, `SpawnableEntitiesManager.cpp` | B |
| V-38 | `AZ::Job` vs `AZ::TaskGraph` as the preferred job API | 04 §3.4 | `AzCore/Jobs/`, `AzCore/Task/` | — |
| V-39 | Allocator set after the rework: `SystemAllocator`, `PoolAllocator`, `ThreadPoolAllocator`, `AZ_CLASS_ALLOCATOR` | 04 §3.5 | `Code/Framework/AzCore/AzCore/Memory/` | B |
| V-40 | Allocation records / memory tracking enable key; fragmentation stats | 03 §2.1, 04 §3.5 | `AzCore/Memory/AllocationRecords.h`, settings registry keys under `/O3DE/Memory` (search) | — |
| V-41 | O3DE process memory stats API for HUD | 05 §4.1 | `AzCore/Memory/`, `AzCore/Debug/`; Profiler/ImGui memory panels | — |
| V-42 | EBus traits (`HandlerPolicy`, `AddressPolicy` static constexpr), `AZ::EBus<>` alias; `AZ::Interface<>` preference | 05 §4.2 bus header | `AzCore/EBus/EBus.h`, `Policies.h`; `AzCore/Interface/Interface.h` | B |
| V-43 | `AZ_COMPONENT` vs `AZ_COMPONENT_DECL/IMPL`; service function signatures; `AZ_CRC_CE` | 05 §4.2 component header | `AzCore/Component/Component.h`; DefaultGem template SystemComponent | B |
| V-44 | `TickBus::Handler::OnTick(float, AZ::ScriptTimePoint)`, `GetTickOrder`, physics tick placement | 05 §4.2 | `AzCore/Component/TickBus.h`; `Gems/PhysX/Core/Code/Source/System/` | B |
| V-45 | AzCore include paths used in headers | 05 §4.2 | Compile test (G10) | B |
| V-46 | `AZ_TYPE_INFO_SPECIALIZE` for reflecting an external (std-only) struct | ChunkStreamingConfig.h | `AzCore/RTTI/TypeInfo.h` | — |
| V-47 | Launcher command-line/CVar for auto-running a benchmark route | 05 §4.1 | `AzCore/Console/IConsole.h`; `Code/LauncherUnified` | — |
| **Rendering (Atom)** | | | | |
| V-48 | Minimum DX12 feature level | 04 §3.0 | docs requirements; `Gems/Atom/RHI/DX12/` device init | — |
| V-49 | Atom render threading model (RPI on main, job-recorded command lists) | 04 §3.0 | `Gems/Atom/RPI/`, `Gems/Atom/RHI/Code/Source/RHI/FrameScheduler.cpp` | — |
| V-50 | No camera-relative rendering; float world positions in object constants | 04 §3.1 | `Gems/Atom/Feature/Common/Code/Source/Mesh/`, view SRG | — |
| V-51 | MeshFeatureProcessor public API usable from a gem for entity-less meshes; thread-safety | 04 §3.3, §3.4 | `Gems/Atom/Feature/Common/Code/Include/Atom/Feature/Mesh/MeshFeatureProcessorInterface.h` | **B** |
| V-52 | GPU instancing of identical mesh+material draws | 04 §3.8 | Same folder (`MeshInstanceManager` or similar); 23.x+ release notes | — |
| V-53 | Visibility system (octree / IVisibilityScene) for frustum culling | 04 §3.6 | `AzFramework/Visibility/` | — |
| V-54 | Occlusion Culling Plane component + Masked Occlusion Culling | 04 §3.6 | `Gems/Atom/Feature/Common/Code/Source/OcclusionCullingPlane/`; RPI `Culling.cpp` | — |
| V-55 | Mesh LOD config: min screen coverage / culling | 04 §3.6, §3.7 | `Gems/Atom/RPI/Code/Include/Atom/RPI.Public/Culling.h`; mesh component LOD settings | — |
| V-56 | Per-cascade shadow culling; separate shadow LOD | 04 §3.6, §3.7 | `Gems/Atom/Feature/Common/Code/Source/Shadows/`, `CoreLights/DirectionalLightFeatureProcessor` | — |
| V-57 | Light culling model and limits (clustered/forward+) | 01 §1.3, 04 §3.8 | `Gems/Atom/Feature/Common/Code/Source/CoreLights/`; docs Atom lights | — |
| V-58 | Dithered LOD cross-fade support | 04 §3.4, §3.7 | Atom mesh LOD code; StandardPBR shader options | — |
| V-59 | Streaming image pool budget + mip bias/priority controls; force-resident mips (inspect mode) | 01 V29, 04 §3.5, §3.7 | `Gems/Atom/RPI/Code/Source/RPI.Public/Image/StreamingImagePool*`, `ImageSystem` | — |
| V-60 | Reflection probe bake format/compression | 04 §3.5 | `Gems/Atom/Feature/Common/Code/Source/ReflectionProbe/` | — |
| V-61 | Diffuse GI probe grid cost | 04 §3.8 | `Gems/DiffuseProbeGrid/` | — |
| V-62 | Pass attachment memory view / RHI memory stats | 04 §3.5, 05 §4.1 | Atom ImGui tools (`Gems/Atom/Utils/`), RHI memory statistics | — |
| V-63 | Draw-call / triangle stats exposed | 05 §4.1 | Atom ImGui "Pass tree" / RPI stats | — |
| V-64 | Buffer pool sub-allocation (fragmentation) | 04 §3.5 | `Gems/Atom/RHI/Code/Source/RHI/BufferPool*` | — |
| V-65 | Video playback support (security footage alternative) | 01 AD-14 | Gem catalog (video/media gems) | — |
| V-66 | Render-to-texture for phone camera (deferred) | 01 V34 | Atom RPI render pipeline / attachment image docs | — |
| V-67 | Third-party / gem ocean water | 01 AD-02 | Gem catalog; community gems | — |
| **Physics** | | | | |
| V-68 | AzPhysics scene API: add/remove static actors without entities; write threading | 04 §3.3, §3.4 | `Code/Framework/AzFramework/AzFramework/Physics/PhysicsScene.h`; `Gems/PhysX/Core/Code/Source/Scene/` | **B** |
| V-69 | PhysX 5 default sleep threshold in O3DE | 04 §3.1 | `Gems/PhysX/Core/Code/Source/RigidBody.cpp` / rigid body configuration | — |
| V-70 | PhysX origin shift not exposed | 04 §3.1 | Search `shiftOrigin` in `Gems/PhysX` | — |
| V-71 | No double-precision world build | 04 §3.1 | `AzCore/Math/Vector3.h` (float `Simd` types); docs/RFCs on large worlds | — |
| V-72 | CCD configuration for fast vehicles | 01 §1.3 | `Gems/PhysX/Core` rigid body config (CCD flags) | — |
| V-73 | PhysX character controller component | 01 V01 | `Gems/PhysX/Core/Code/Source/` CharacterController | — |
| V-74 | Scene raycast API (weapons, flood depth) | 01 V23, 04 §3.9 | `AzFramework/Physics/PhysicsScene.h` (`QueryScene`) | — |
| V-75 | No stock vehicle gem (or maturity of any existing one) | 01 V11 | Gem catalog | B (P2 gameplay) |
| **Other gems** | | | | |
| V-76 | Profiler gem + `AZ_PROFILE_SCOPE` macro names | 03 §2.1 | `Gems/Profiler/`; `AzCore/Debug/Profiler.h` | — |
| V-77 | ImGui gem available in GameLauncher profile builds | 05 §4.1 | `Gems/ImGui/` | B |
| V-78 | Terrain gem runtime streaming of spawner areas (rejected path) | 04 §3.2 | `Gems/Terrain/` | — |
| V-79 | RecastNavigation runtime tile add/remove (P3) | 04 §3.2 | `Gems/RecastNavigation/` | — |
| V-80 | EMotionFX: root motion, anim events | 01 V03, V21 | `Gems/EMotionFX/` | — |
| V-81 | UI stack (LyShine vs. newer) | 01 V33 | `Gems/LyShine/`; 26.05 release notes | — |
| V-82 | Spline component | 01 V58 | `Code/Framework/AzFramework` / `Gems/LmbrCentral/Code/Source/Shape/SplineComponent*` | — |
| V-83 | Light-level sampling for stealth (P3) | 01 V26 | Atom lights query / custom | — |
| V-84 | Atom runtime material property API (paint) | 01 V15 | `Gems/Atom/Feature/Common/.../Material/MaterialComponentBus.h` | — |
| V-85 | SerializeContext version converters (tooling export only) | 04 §3.10 | `AzCore/Serialization/SerializeContext.h` | — |
| **External / policy** | | | | |
| V-86 | Steam adult-content policy (2025 changes) | 01 §1.4 | Steamworks content survey docs | — |

**P2-blocking summary (23 rows marked B):**
- V-01–V-04, V-08, V-10
- V-22, V-27, V-28, V-30, V-31, V-34
- V-35, V-37, V-39, V-42–V-45
- **V-51**, **V-68** (bulk path, R3)
- V-75 (gameplay P2)
- V-77

Tasks 1–10 above cover all except:
- **V-30, V-31, V-35, V-39**: Week 5 spike covering the ChunkData builder + runtime asset handler, AZ::IO Streamer deadlines/cancel, and allocator wiring for the block pool.
- **V-75**: first item of P2 gameplay prototyping.
