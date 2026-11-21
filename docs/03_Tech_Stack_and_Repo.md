# FILE: /docs/03_Tech_Stack_and_Repo.md

**Project:** LOW TIDE | **Phase:** 1 | **Doc status:** v0.1

The literal files are the source of truth and are **not duplicated here** (this avoids drift):
- [/.gitignore](../.gitignore)
- [/.gitattributes](../.gitattributes)
- [/tools/templates/CMakePresets.json](../tools/templates/CMakePresets.json). It is copied into `/LowTide/` after project creation (§2.5).

---

## 2.1 Stack Lock

**Fact:** O3DE **26.05** is the latest confirmed stable release (May 2026). 26.10 is planned but not confirmed shipped. 26.05 makes PhysX 5 the default for new projects and bundles CMake. Sources: o3de.org download page, 26.05 release notes.

| Item | Locked choice | Rationale (one line) |
|---|---|---|
| Engine | **O3DE 26.05.x**, the latest 26.05 patch at setup [VERIFY patch tag on GitHub releases]. **Built from source** at the pinned tag, cloned **outside** the repo (e.g. `D:\o3de\26.05`) and registered with `o3de register` [VERIFY CLI] | Source lets me debug and patch the streaming path into the engine. Out-of-repo keeps the engine's multi-GB LFS history out of ours |
| Engine upgrade policy | **No engine upgrades inside a phase.** Evaluate 26.10 only at the P2 exit gate, and only for a blocking fix | O3DE API churn is risk #1 for a solo dev |
| Engine patches | Kept as `.patch` files in `/tools/engine_patches/`; target = zero | Re-applicable on upgrade and auditable |
| Physics | PhysX 5 (26.05 default) [VERIFY gem name `PhysX5` vs. `PhysX`] | Default path = most tested |
| Graphics API | **DX12 primary**, Vulkan secondary (smoke-tested monthly) | Windows-only target; PIX gives the best DX12 tooling |
| IDE / compiler | **Visual Studio 2022 17.14** (MSVC v143 toolset), x64 [VERIFY 26.05 lists VS2022 as supported; VS2026 support unknown] | Officially supported generator; 17.14 is the long-lived final 2022 band |
| Windows SDK | 10.0.22621 or newer (engine floor: 10.0.19041) | Meets the documented floor with headroom |
| CMake | **Engine-bundled CMake** (26.05) [VERIFY bundled version ≥ 3.25, required by presets v6]; fallback: install CMake ≥ 3.25 | One less version to manage |
| C++ standard | Engine-set standard [VERIFY 17 vs. 20 in 26.05 `cmake/` config]. **Our code is written to the C++17 subset** (no concepts, ranges, modules or coroutines) | Compiles unchanged whichever standard the engine sets, and survives upgrades |
| Python (tooling) | **Engine-bundled Python** (`<engine>/python/python.cmd`) [VERIFY version, historically 3.10.x]. Repo tools are stdlib-only | One interpreter; the same one O3DE editor scripts use |
| DCC | **Blender 4.5 LTS** (FBX export) [VERIFY whether a newer LTS exists at setup; adopt only at a phase boundary] | Free, LTS stability, mature FBX exporter |
| Texture authoring | Deferred to P4 (Scope Flag) | Out of Phase 1 scope |
| VCS | Git ≥ 2.45 + Git LFS ≥ 3.5 | Standard; LFS for all binaries |
| Remote | **GitHub private repo + LFS data packs**, plus a weekly mirror (`git push --mirror` + `git lfs push --all`) to a local NAS/external drive [VERIFY current GitHub LFS quota/pricing] | `gh` tooling. The mirror covers host and quota risk |
| CPU profiling | **O3DE built-in profiler** (Profiler gem, ImGui visualizer, `AZ_PROFILE_SCOPE` markers) [VERIFY gem/macro names in 26.05] | Zero integration cost; the engine is already instrumented |
| GPU profiling / frame debug | **PIX on Windows** (DX12 timing, GPU captures, memory). **RenderDoc** (frame debugging, both APIs) | Both free. PIX is authoritative for DX12 |
| Vendor tools | Nsight Graphics/Systems only if the dev GPU is NVIDIA. AMD Radeon GPU Profiler if AMD | Use whatever matches the hardware |
| I/O / thread timeline | Windows Performance Recorder/Analyzer (ETW) | Validates streaming I/O under load without code changes |
| Memory | O3DE allocator tracking [VERIFY 26.05 mechanism/CVar] + per-chunk arena tags (Part 3 §3.5) + MSVC ASan (`win-vs2022-asan` preset) [VERIFY O3DE runs under ASan with its allocators] | Leak and corruption detection across thousands of load/unload cycles |
| Tracy | **Not adopted in P1.** Re-evaluate at P2 week 2 if the built-in profiler can't show streaming job timelines clearly | Needs an integration gem plus maintenance |
| Code format | `.clang-format` copied from the pinned engine tag | Matches engine style; no bikeshedding |

**Dev machine (ASSUMPTION):** ≥ 300 GB free NVMe. The engine source plus profile build, 3rdParty, Cache and repo is estimated at 150–200 GB, plus headroom. Measure after the first full build.

---

## 2.2 Repo Layout

The repo root is the current `my-game/` folder. Rename it to `low-tide/` before `git init` if you like; nothing depends on the root name. Delete the empty `my-game/package.json`, which has no role in an O3DE project.

```text
<repo-root>/
├── .gitattributes                  # LFS rules + line endings (literal file)
├── .gitignore                      # O3DE/IDE/DCC ignores (literal file)
├── .clang-format                   # copied from pinned engine tag
├── .editorconfig                   # whitespace rules for non-C++ files
├── README.md                       # setup steps (§2.5) and stack lock summary
├── docs/                           # design docs 01–06, ADRs, [VERIFY] index
│   └── adr/                        # one file per architecture decision (ADR-NNN-title.md)
├── tools/                          # repo tooling (stdlib Python, engine-bundled interpreter)
│   ├── hooks/                      # git hooks (core.hooksPath target)
│   ├── validate_names.py           # naming validator (§2.4; spec only in P1)
│   ├── chunk_budget_report.py      # per-chunk budget report (spec in 04 §3.8)
│   ├── testbed_gen/                # streaming testbed chunk-prefab generator (spec in 05)
│   ├── engine_patches/             # *.patch against the pinned engine tag (target: empty)
│   └── templates/                  # CMakePresets.json, gem/file templates
├── tests/                          # non-unit test assets and harness data
│   ├── benchmarks/                 # scripted fly-through routes, pass/fail thresholds
│   ├── soak/                       # soak-test configs (30-min, 2-h)
│   ├── tools/                      # pytest-free unittest suites for /tools scripts
│   ├── core_standalone/            # CMake project building streaming Core + tests WITHOUT the engine
│   └── results/                    # committed summary CSVs; raw captures ignored
├── assets_source/                  # RAW DCC files - NOT scanned by Asset Processor
│   ├── blender/                    # .blend working files, mirrors Assets/ folder structure
│   ├── substance/                  # .spp/.sbs working files
│   ├── textures_raw/               # .psd/.exr/.tif masters before export
│   ├── audio/                      # session files and uncompressed masters
│   └── reference/                  # reference images, blockout sketches (low-priority LFS)
└── LowTide/                        # O3DE PROJECT FOLDER (created by o3de create-project)
    ├── project.json                # project manifest: engine name/version pin, gem list
    ├── CMakeLists.txt              # project CMake entry (template-generated)
    ├── CMakePresets.json           # copied from tools/templates
    ├── Gem/                        # PRIMARY GAME GEM "LowTide" (template-generated) [VERIFY folder name]
    │   └── Code/                   # gameplay code (P2+); depends on LowTideStreaming
    ├── Gems/
    │   └── LowTideStreaming/       # STREAMING GEM (separate; justified below)
    │       ├── gem.json
    │       ├── CMakeLists.txt
    │       └── Code/
    │           ├── Include/LowTideStreaming/  # public headers (interfaces, buses, handles)
    │           ├── Source/Core/               # engine-independent logic (std-only C++)
    │           ├── Source/Engine/             # O3DE glue: components, EBus handlers, I/O, spawning
    │           ├── Tests/                     # AzTest/GoogleTest unit + integration tests
    │           └── *_files.cmake              # source lists [VERIFY template naming]
    ├── Assets/                     # ENGINE-INGESTED sources (exported FBX/PNG/WAV, .material, .prefab)
    │   ├── Characters/             # SK_ meshes, A_ animations, per character folder
    │   ├── Vehicles/               # one folder per vehicle archetype
    │   ├── Kits/                   # modular building kits, per district style
    │   ├── Props/                  # props by category
    │   ├── Materials/              # M_ masters (shared); MI_ live next to their asset
    │   ├── Audio/                  # SFX_/VO_/MUS_/AMB_ exported files
    │   ├── FX/                     # FX_ particle assets (P4)
    │   ├── UI/                     # UI_ canvases/textures (P3+)
    │   ├── World/
    │   │   ├── Chunks/<DistrictCode>/Chunk_Xnnn_Ynnn/  # per-chunk prefabs + chunk data
    │   │   ├── Cells/              # interior cells (Cell_)
    │   │   ├── Vignettes/          # vignette cells (Vig_)
    │   │   ├── HLOD/               # far-tier proxies + skyline backdrop
    │   │   └── Global/             # always-resident world data (road graph, water, TOD presets)
    │   └── Data/                   # DT_ data tables (missions/dialogue later; streaming config now)
    ├── Levels/                     # L_ levels: L_World, L_StreamingTestbed, L_Vig_*
    ├── Registry/                   # .setreg settings (streaming tunables, image builder presets)
    ├── Platform/                   # template-generated platform config
    ├── Cache/                      # IGNORED - Asset Processor products
    ├── user/                       # IGNORED - per-user data, logs
    └── build/                      # IGNORED - CMake trees per preset
```

**Streaming gets its own gem: YES.**
- *Why:* it enforces dependency direction (game → streaming, never the reverse). It isolates `Source/Core` (std-only) so the logic tests build and run standalone in seconds (`tests/core_standalone`). It can be loaded by the testbed level without gameplay code. Each gem compiles on its own.
- *Cost:* an extra gem.json and CMake boilerplate, plus cross-gem EBus boundaries.
- *Rejected:* streaming inside the game gem, which invites gameplay↔streaming coupling and slower tests. Splitting into more than two gems (Core gem + Engine gem) adds boilerplate with no isolation gain over a folder split.

**Assets/ vs assets_source/:**
- The Asset Processor only scans the project's `Assets/` (and gems).
- `assets_source/` sits outside `LowTide/`, so it is never scanned [VERIFY default scan folders in `AssetProcessorPlatformConfig.setreg`].
- Exported files are committed in `Assets/`. Working files are committed in `assets_source/`. Both are LFS.

---

## 2.3 Git Configuration

### Literal files
[/.gitignore](../.gitignore) and [/.gitattributes](../.gitattributes). Key decisions:
- `.prefab`, `.material`, `.assetinfo` and `.setreg` are **text, not LFS**. They are JSON and must stay diffable and mergeable.
- `.assetinfo` (scene and texture import settings) must be committed, not ignored.
- `.obj` is LFS (Wavefront mesh) and deliberately *not* globally ignored. MSVC `.obj` files only appear under the ignored `build/`.
- LFS files are **not lockable**. Locking makes files read-only on checkout, which is pure friction for a solo dev.

### Initialization (run once, from the repo root, Windows shell)

```bat
git init -b main
git config core.autocrlf false
git config core.longpaths true
git config core.hooksPath tools/hooks
git config lfs.locksverify false
git lfs install --local
git add .gitattributes .gitignore
git commit -m "chore(repo): add gitattributes (LFS rules) and gitignore"
git lfs track
git check-attr filter -- LowTide/Assets/test.fbx assets_source/blender/test.blend docs/x.md
```

Then add docs/tools, create the O3DE project (§2.5), commit, and push:

```bat
gh repo create low-tide --private --source . --remote origin
git push -u origin main
```

**LFS verification (Exit Gate item):**

```bat
:: commit a known binary first, e.g. assets_source/reference/LFS_Probe.png
git lfs ls-files
cd %TEMP% && git clone <origin-url> lt_verify && cd lt_verify
git lfs ls-files
git lfs fsck
certutil -hashfile assets_source\reference\LFS_Probe.png SHA256
```

Pass condition: the hash matches the original file. `git lfs fsck` reports OK. No file in the clean clone is a ~130-byte pointer stub.

### Branching strategy (trunk-based, solo)

| Rule | Value |
|---|---|
| Trunk | `main`. Always builds the `profile` preset and passes the core tests |
| Feature branches | `feat/<scope>-<short-desc>`, `fix/…`, `exp/…` (experiments, may be deleted unmerged) |
| Branch lifetime | ≤ 3 days. Longer work is hidden behind a CVar/feature flag and merged into `main` |
| Merge method | Rebase + fast-forward (linear history). Squash `exp/` branches |
| Asset-heavy commits | Separate from code commits (keeps bisect usable) |
| No `develop` branch, no release branches before P6 | Overhead with no benefit for one person |

### Commit message convention (Conventional Commits)

`<type>(<scope>): <imperative summary ≤ 72 chars>`
- types: `feat fix perf refactor test docs build chore asset revert`
- scopes: `stream core gem game tools tests docs build assets world`

Example: `perf(stream): time-slice instantiation to 2 ms budget`.
Body: why plus measured numbers when the commit is perf-related. Footer: `Refs: ADR-004` where applicable.

### Tagging scheme (annotated tags only)

| Tag | Meaning |
|---|---|
| `v0.1.0` | Phase 1 exit (design complete, repo verified) |
| `v0.2.0` | P2 exit (graybox prototype + streaming testbed passing) |
| `v0.3.0` | P3 exit |
| `v0.4.0` | P4 exit = **MVS** |
| `v0.5.x` | P5 content milestones |
| `v1.0.0-rcN` / `v1.0.0` | P6 release candidates / release |
| `bench/YYYYMMDD-<preset>` | Lightweight tag on a commit whose benchmark results are committed to `tests/results/` |

Command: `git tag -a v0.1.0 -m "Phase 1 exit: design + repo"` then `git push origin v0.1.0`.

---

## 2.4 Naming Conventions

### General rules (all assets)
1. Format: `<Prefix>_<Scope>_<Name>[_<Variant>][_<Suffix>]`.
2. Segments are **PascalCase**, ASCII `[A-Za-z0-9]` only, separated by a single `_`. No spaces, hyphens or Unicode.
3. **Name ≤ 64 characters** (excluding extension). Derivation: Windows MAX_PATH 260 − repo root (~40) − Cache product path expansion (~60) − deepest folder chain (~80) ≈ 80. The 64 limit leaves margin. `core.longpaths` stays on as a backstop, not a licence.
4. `Scope` = district code (`GM` Glass Mile, `OM` Old Mercy, `TC` The Cut, `BC` Briar County, `PK` Pelican Keys, `NB` Northbank), `Kit<Name>`, or a category (`Street`, `Interior`, `Veh`, `Char`).
5. Variants: `_A`, `_B`, … (visual), `_Dmg1`, `_Dmg2` (damage stages), `_Night` / `_Storm` (state variants, AD-09).
6. The file name and the in-engine asset name are identical. The folder may not override the prefix.

### Asset prefixes / suffixes

| Type | Prefix | Required suffix(es) | Extension(s) | Example |
|---|---|---|---|---|
| Static mesh | `SM_` | mesh nodes `_LOD0…_LOD3`, collision node `_COL` [VERIFY O3DE scene-builder LOD/physics node naming rules] | .fbx | `SM_OM_Storefront_A.fbx` |
| Skeletal mesh | `SK_` | `_LOD0…` | .fbx | `SK_Char_TylerReed.fbx` |
| Animation | `A_` | — | .fbx | `A_Char_Melee_Jab_L.fbx` |
| Anim graph / motion set | `AG_` / `MS_` | — | .animgraph / .motionset | `AG_Char_Locomotion.animgraph` |
| Master material | `M_` | — | .material (StandardPBR-based) | `M_Master_Opaque.material` |
| Material instance | `MI_` | — | .material (parent = an `M_`) [VERIFY Atom parent-material inheritance] | `MI_OM_Brick_Red.material` |
| Custom material type | `MT_` | — | .materialtype (P4+, likely none) | `MT_Water_Global.materialtype` |
| Texture | `T_` | `_BC` base color, `_N` normal, `_ORM` occl/rough/metal, `_E` emissive, `_M` mask, `_H` height, `_O` opacity | .png / .tif / .exr | `T_OM_Brick_Red_ORM.png` |
| Prefab | `PF_` | — | .prefab | `PF_OM_StreetLamp_A.prefab` |
| Chunk prefab | `PF_Chunk_` | `_<Layer>` (Static, Props, Gameplay, Lights, Audio) | .prefab | `PF_Chunk_X012_Y007_Static.prefab` |
| Interior cell | `Cell_` | `_<Layer>` | .prefab | `Cell_OM_MackWorkshop_Static.prefab` |
| Vignette cell | `Vig_` | — | .prefab | `Vig_Pro_PrisonGate.prefab` |
| HLOD / skyline proxy | `HL_` | `_T1`, `_T2` (tier) | .fbx | `HL_Chunk_X012_Y007_T1.fbx`, `HL_Skyline_NB_T2.fbx` |
| Level | `L_` | — | level folder/prefab | `L_World`, `L_StreamingTestbed` |
| Physics material | `PM_` | — | .physxmaterial [VERIFY extension] | `PM_Asphalt_Wet.physxmaterial` |
| SFX / VO / music / ambience | `SFX_` / `VO_` / `MUS_` / `AMB_` | VO: `VO_<Char>_<MissionId>_<LineNNN>` | .wav / .ogg | `VO_Lena_P00_L014.wav` |
| VFX | `FX_` | — | engine particle asset (P4) [VERIFY 26.05 particle asset type] | `FX_Splash_Vehicle_A` |
| UI | `UI_` | — | .uicanvas / .png | `UI_HUD_StreamingDebug.uicanvas` |
| Data table | `DT_` | — | .json / .setreg | `DT_StreamingConfig_Default.setreg` |
| Sequence/cutscene | `SEQ_` | — | engine sequence asset [VERIFY] | `SEQ_P00_BusArrival` |
| Spline data | `SPL_` | — | .json | `SPL_P00_BusRoute.json` |
| Source files | same name as the export | — | .blend/.spp/.psd | `assets_source/blender/Props/SM_OM_Storefront_A.blend` |

**Texture suffix → O3DE image preset mapping:** the O3DE image builder selects presets by filename suffix. Our `_BC/_N/_ORM/...` suffixes must be mapped to presets in a project `.setreg` override, or set per-file via `.assetinfo` [VERIFY image builder suffix-mapping config location and keys in 26.05]. **This is a P2 day-1 task.** If the suffixes aren't mapped, normals import as sRGB color, which is silently wrong.

### Chunk / cell / ID naming
- Chunk: `Chunk_X<nnn>_Y<nnn>`, zero-padded to 3 digits. Origin = **south-west corner** of the world grid. +X = east, +Y = north. Coordinates are grid indices, not meters (chunk size is set in Part 3).
- Chunk folder: `Assets/World/Chunks/<DistrictCode>/Chunk_X012_Y007/`. The district is assigned by the chunk's majority area and is used for folder organisation only. The ID is the coordinate.
- Mission IDs: `P00` (Prologue), `M01`…`M17`, `A1E`, `A2E`. Chains: `C<Char><nn>` (e.g. `CLena03`).
- Stable entity IDs for AD-09 deltas: assigned by the editor/prefab system and never hand-written [VERIFY prefab entity id stability across edits].

### C++ naming (matches O3DE style)

| Element | Rule | Example |
|---|---|---|
| Namespaces | `LowTide` root, nested by gem: `LowTide::Streaming`. Core logic: `LowTide::Streaming::Core` | `namespace LowTide::Streaming` |
| Classes / structs | PascalCase | `ChunkBudgetTracker` |
| Interfaces (pure abstract) | `I` + PascalCase | `ILodPolicy` |
| EBus interface / bus | `<Name>Requests` / `<Name>RequestBus`, `<Name>Notifications` / `<Name>NotificationBus` (O3DE convention) | `ChunkStreamingRequests`, `ChunkStreamingRequestBus` |
| Components | Suffix `Component`; editor variants prefix `Editor` | `StreamingSystemComponent` |
| Methods / free functions | PascalCase | `RequestPin()` |
| Members / statics / globals | `m_camelCase` / `s_camelCase` / `g_camelCase` | `m_streamInRadiusMeters` |
| Locals / parameters | camelCase | `chunkCoord` |
| Constants (`constexpr`) | PascalCase | `MaxChunksResident` |
| Enums | `enum class` PascalCase, values PascalCase; explicit underlying type | `enum class ChunkState : uint8_t { Unloaded, … }` |
| Units in names | Numeric tunables carry a unit suffix: `Meters`, `Ms`, `Bytes`, `Mps` | `m_ioBudgetBytesPerFrame` |
| Booleans | `Is`/`Has`/`Should` prefix | `IsMissionPinned()` |
| Files | One primary type per file. File name = type name. `.h` / `.cpp` / `.inl` | `ChunkHandle.h` |
| Macros | `LT_` + UPPER_SNAKE (avoid unless required) | `LT_STREAM_ASSERT` |
| Tests | `<Type>Tests.cpp`. Test names `<Unit>_<Condition>_<Expected>` | `ChunkHandle_StaleGeneration_ResolvesNull` |

### Gem naming
PascalCase, prefixed `LowTide`: `LowTide` (project/game gem), `LowTideStreaming`. CMake targets follow the engine's gem target pattern (`<Gem>.Static`, `<Gem>`, `<Gem>.Editor`, `<Gem>.Tests`) [VERIFY current gem template target names].

### Examples

| ✅ Good | ❌ Bad (rule broken) |
|---|---|
| `SM_OM_Storefront_A.fbx` | `storefront_final2.fbx` (no prefix, lowercase, revision in name) |
| `T_TC_Container_Rust_N.png` | `T_TC_Container_Rust_Normal.png` (non-standard suffix → wrong image preset) |
| `PF_Chunk_X012_Y007_Props.prefab` | `PF_Chunk_12_7.prefab` (no padding, no axis letters, no layer) |
| `VO_Mack_M02_L031.wav` | `VO Mack line 31.wav` (spaces, no mission ID) |
| `MI_GM_Neon_Pink_A.material` | `MI_GlassMileNeonPinkVariantANightVersionForTheSaintExterior.material` (> 64 chars, scope not coded) |

### Enforcement (spec only; implemented in P2 week 1)

**`tools/validate_names.py`**
- Modes: `--staged` (git staged files), `--all`, `--path <dir>`.
- Rules table: extension + folder → required prefix regex, suffix set, max length, ASCII set.
- Extra checks:
  - Chunk coordinates are inside the world grid bounds (read from `DT_StreamingConfig_Default.setreg`).
  - Every `T_` has a recognised suffix.
  - Every `SM_` export has a sibling `.assetinfo` (LOD and collision node checks are done in the LOD validator, Part 3 §3.7).
  - No `assets_source/` file lacks a matching export, *warn only*.
  - No binary extension is missing an LFS attribute (`git check-attr filter`).
- Output: one line per violation as `path: RULE_ID message`. Exit code 1 on any error.
- Allowlist file `tools/naming_allowlist.txt`. Each entry needs a reason; engine-template files are exempt.

**Pre-commit hook outline (`tools/hooks/pre-commit`, enabled via `core.hooksPath`):**
1. Run `validate_names.py --staged` with the engine-bundled Python.
2. Run `clang-format --dry-run --Werror` on staged `*.h/*.cpp/*.inl`.
3. Reject staged non-LFS files > 5 MB (catches LFS misconfiguration).
4. Exit non-zero on any failure. `--no-verify` is allowed only for `exp/` branches.

---

## 2.5 Build and CI-lite

### Project creation (one-time, after engine build)

1. Build the engine at the pinned tag per the O3DE docs and register it: `scripts\o3de.bat register --this-engine` [VERIFY].
2. Create the project: `scripts\o3de.bat create-project --project-path <repo>\LowTide --template-name DefaultProject` [VERIFY flags/template name in 26.05].
3. Create the streaming gem: `scripts\o3de.bat create-gem --gem-path <repo>\LowTide\Gems\LowTideStreaming` and register/enable it on the project [VERIFY `register --gem-path` / `enable-gem` subcommands].
4. Copy `tools/templates/CMakePresets.json` to `LowTide/CMakePresets.json`.
5. Commit: `build(repo): create O3DE project and streaming gem`.

### Presets ([template](../tools/templates/CMakePresets.json))

| Preset | Configs | Use |
|---|---|---|
| `win-vs2022` (configure) | debug; profile; release | Daily dev. Multi-config VS generator, unity build on [VERIFY `LY_UNITY_BUILD`] |
| `win-vs2022-mono` | release | Shipping-shaped monolithic launcher [VERIFY `LY_MONOLITHIC_GAME`] |
| `win-vs2022-asan` | debug | ASan runs of launcher + tests. Unity off for clearer reports |
| build `profile` | — | **Default daily config**: optimised + profiler markers + asserts. Targets Editor, GameLauncher, AssetProcessorBatch [VERIFY target names] |
| build `debug` | — | Only when stepping through code (much slower) |
| build `release` | — | Weekly smoke from P5. Before that, only at phase exits |
| test `profile-tests` | — | ctest over LowTide-named tests [VERIFY O3DE test labels/naming, e.g. `SUITE_main`] |

Configuration semantics (O3DE): **debug** = no optimisation, full checks. **profile** = optimised with debug info and profiling enabled. **release** = optimised, profiling and most asserts stripped [VERIFY exact define set per config].

Commands:
```bat
cd LowTide
cmake --preset win-vs2022
cmake --build --preset profile -- /m
ctest --preset profile-tests
```

Standalone core tests (no engine, seconds to build):
```bat
cmake -S tests\core_standalone -B build\core -G "Visual Studio 17 2022"
cmake --build build\core --config Release && ctest --test-dir build\core -C Release
```

### Local pre-push checklist (no cloud CI)

| # | Check | Command / method | Gate |
|---|---|---|---|
| 1 | Standalone core tests pass | `ctest --test-dir build\core -C Release` | Must |
| 2 | `profile` build succeeds (Editor + Launcher) | `cmake --build --preset profile` | Must |
| 3 | Gem tests pass | `ctest --preset profile-tests` | Must |
| 4 | Naming validator clean | `validate_names.py --all` | Must |
| 5 | Asset Processor batch has 0 failed jobs | `AssetProcessorBatch.exe` [VERIFY exe/flags] → log scan | Must when `Assets/` changed |
| 6 | LFS integrity | `git lfs fsck` + no staged non-LFS binary > 5 MB | Must when binaries changed |
| 7 | Streaming benchmark smoke (from P2) | 2-min testbed route; p99 and hitch thresholds from 05 | Must when `Gems/LowTideStreaming` changed |
| 8 | ASan run of core + gem tests | `asan` preset | Weekly, and before every phase tag |
| 9 | Mirror push | `git push --mirror nas` + `git lfs push --all nas` | Weekly |
