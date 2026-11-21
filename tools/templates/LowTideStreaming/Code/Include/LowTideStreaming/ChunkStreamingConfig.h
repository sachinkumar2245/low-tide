/*
 * LowTideStreaming - ChunkStreamingConfig.h
 *
 * Every streaming tunable from docs/04 as one value struct with defaults.
 * All defaults are TUNABLE STARTING VALUES [T] to be validated by the testbed (docs/05).
 * Runtime source of truth: Registry/DT_StreamingConfig_Default.setreg (loaded by engine glue,
 * which overrides these defaults). Units are encoded in member names.
 *
 * DEPENDENCIES: C++17 standard library only. Reflection/serialization is provided by the engine
 * glue (Source/Engine), not in this header. Reflection TypeId:
 * {1FCB43A2-B89D-4CC2-9F2A-2B9C0D8C08A5}, declared in the glue with
 * AZ_TYPE_INFO_SPECIALIZE(LowTide::Streaming::ChunkStreamingConfig, "{...}") placed inside
 * namespace AZ (lookup is via AZ::Adl; docs/06 V-46).
 *
 * THREADING CONTRACT:
 *   Immutable after StreamingSystemComponent::Activate. Readable from any thread without locks.
 *   Hot reload (debug only) replaces the whole struct on the main thread between frames; worker
 *   jobs receive a copy of the fields they need at dispatch.
 *
 * VALIDATION (performed by Validate(); failure = refuse to activate and log):
 *   - gridDimX/Y in [1, MaxGridDimension]
 *   - half-diagonal of grid (meters) <= precisionLimitMeters (4 096)
 *   - per layer: radiusInMeters < radiusOutMeters < radiusEvictMeters
 *   - budgets > 0; blockCount * blockSizeBytes <= ramBudget.chunkArenaPoolBytes
 */
#pragma once

#include <LowTideStreaming/ChunkTypes.h>

#include <cstddef>
#include <cstdint>

namespace LowTide::Streaming
{
    //! Load/unload radii for one layer (meters to chunk 2D AABB). See 04 §3.3 hysteresis table.
    struct LayerRadii
    {
        float radiusInMeters = 0.0f;
        float radiusOutMeters = 0.0f;
        float radiusEvictMeters = 0.0f;
        float minDwellSeconds = 0.0f;
        float priorityLayerFactor = 1.0f;   //!< multiplies ETA in the priority key
    };

    struct ChunkStreamingConfig
    {
        // --- Grid / world (04 §3.1, §3.2) -----------------------------------------------------
        float chunkSizeMeters = 128.0f;
        uint16_t gridDimX = 32;
        uint16_t gridDimY = 32;
        float precisionLimitMeters = 4096.0f;        //!< max |pos| from origin (ULP <= 0.244 mm)
        float positionGuardMeters = 3000.0f;         //!< debug assert threshold for any entity
        float crossBorderToleranceMeters = 16.0f;    //!< authored object bounds may exceed chunk by this

        // --- Layer radii (04 §3.3) --------------------------------------------------------------
        LayerRadii layers[ChunkLayerCount] = {
            /* Gameplay     */ {256.0f, 320.0f, 512.0f, 3.0f, 1.0f},
            /* StaticVisual */ {384.0f, 448.0f, 640.0f, 3.0f, 1.5f},
            /* Detail       */ {128.0f, 192.0f, 320.0f, 2.0f, 3.0f},
            /* Interior     */ {48.0f, 64.0f, 128.0f, 2.0f, 1.0f},   //!< measured from portal
            /* Hlod1        */ {2000.0f, 2200.0f, 2400.0f, 5.0f, 4.0f},
        };
        float airlockPreloadMeters = 60.0f;

        // --- Priority / prediction (04 §3.3) ----------------------------------------------------
        float lookAheadSeconds = 3.0f;               //!< forward radius growth = v * this * max(0, cos)
        float roadPredictionHorizonSeconds = 3.0f;
        float walkSpeedFloorMps = 1.5f;              //!< ETA denominator floor
        uint16_t maxCandidatesSorted = 128;

        // --- Main-thread budgets (04 §3.0, §3.4) -----------------------------------------------
        // Instantiation cost includes spawn batches processed by SpawnableSystemComponent at
        // TICK_GAME (80) in the same frame (charged by measured EMA), AcquireMesh batches and
        // AddSimulatedBodies batches issued from our tick (order streamingTickOrder).
        float schedulerBudgetMs = 0.2f;
        float instantiateBudgetMs = 1.4f;
        float deactivateBudgetMs = 0.4f;
        float instantiateBudgetUnderDeficitMaxMs = 4.0f;
        float frameTargetMs = 16.6f;
        float workItemMaxMs = 0.25f;                 //!< work items pre-split to at most this cost
        int32_t streamingTickOrder = 78;             //!< after TICK_INPUT (75), before TICK_GAME (80) spawn processing
                                                     //!< and TICK_PHYSICS_SYSTEM (200) simulate (docs/06 V-37, V-44)

        // --- Spawning (04 §3.3) -----------------------------------------------------------------
        uint16_t spawnBatchMaxEntities = 10;         //!< per SpawnEntities(indices) call; whole hierarchies only
        uint8_t spawnBatchesInFlightPerTicket = 1;   //!< invariant: queued spawns cannot be cancelled
        uint8_t spawnPriorityHigh = 32;              //!< <= engine high-lane threshold (default 64)
        uint8_t spawnPriorityDefault = 128;
        float spawnHighLaneEtaSeconds = 2.0f;        //!< G layers with ETA below this use the high lane

        // --- Mesh handles (04 §3.2, §3.4) --------------------------------------------------------
        uint16_t workItemMaxMeshHandles = 50;        //!< AcquireMesh/ReleaseMesh per work item
        uint32_t maxMeshHandlesResident = 24000;     //!< global cap (Atom Simulate walks all handles)
        uint16_t maxStaticSectionsPerChunk = 200;    //!< merged kit sections (S layer), dense urban
        uint16_t maxDetailHandlesPerChunk = 250;     //!< instanced prop handles (D layer), dense urban
        float staticSectionCellMeters = 32.0f;       //!< merge granularity (frustum-culling resolution)
        bool requireMeshInstancing = true;           //!< activation fails if r_meshInstancingEnabled is false

        // --- I/O and jobs (04 §3.4) --------------------------------------------------------------
        uint8_t maxLayersInFlight = 4;
        size_t maxBytesInFlight = 64ull * 1024 * 1024;
        uint8_t maxConcurrentDeserializeJobs = 2;    //!< post-load TaskGraph jobs (parsing itself runs in AssetManager load jobs)
        float deserializeJobMaxMs = 8.0f;
        uint8_t stagingBufferCount = 8;
        size_t stagingBufferBytes = 8ull * 1024 * 1024;
        size_t decompressScratchBytes = 32ull * 1024 * 1024;
        float assumedReadThroughputMBps = 150.0f;    //!< seed for T_ready estimate before EMA warms up
        float readyTimeEmaAlpha = 0.1f;

        // --- Memory (04 §3.5) ---------------------------------------------------------------------
        size_t arenaBlockSizeBytes = 256ull * 1024;
        uint32_t arenaBlockCount = 768;              //!< 192 MB pool
        uint32_t arenaMaxBlocksPerLayer = 16;        //!< 4 MB per layer cap (04 §3.8)
        size_t loadedInactiveCacheBytes = 256ull * 1024 * 1024;
        size_t frameScratchMainBytes = 1ull * 1024 * 1024;
        size_t frameScratchWorkerBytes = 256ull * 1024;
        uint16_t handleSlotCapacity = 2048;
        uint16_t workItemCapacity = 512;

        // --- Timeouts / retries (04 §3.3) -------------------------------------------------------
        float ioTimeoutSeconds = 5.0f;
        float deserializeTimeoutSeconds = 1.0f;
        float instantiateWarnSeconds = 10.0f;
        uint8_t ioMaxRetries = 2;
        float retryBackoffSeconds[2] = {0.5f, 2.0f};
        float failedRetryDelaySeconds = 10.0f;

        // --- Degradation ladder (04 §3.4) -------------------------------------------------------
        float l1SlackSeconds = 1.5f;
        float l2SlackSeconds = 0.75f;
        float l3SlackSeconds = 0.3f;
        float softCapMinSpeedMps = 15.0f;
        float softCapMarginMeters = 30.0f;
        float l4StallDistanceMeters = 16.0f;
        float vehicleHardCapMps = 55.0f;             //!< AD-06 (owned by vehicle system; mirrored for checks)
        float boatHardCapMps = 25.0f;                //!< AD-11

        // --- HLOD / LOD / culling (04 §3.6, §3.7) ---------------------------------------------
        // Per-class LOD settings live in ILodPolicy (Atom LOD is per mesh; no hysteresis exists).
        uint8_t hlodClusterDim = 4;                  //!< T2 cluster = 4x4 chunks
        float hlodT2SwitchMeters = 1000.0f;
        float terrainLodRingMeters[3] = {128.0f, 256.0f, 384.0f}; //!< forced terrain LOD 0/1/2 boundaries
        uint8_t maxActiveOccluders = 32;             //!< occluder planes enabled per frame
        uint8_t maxShadowedLocalLightsGlobal = 8;
        float interiorPortalMaxMeters = 64.0f;

        // --- Persistence (04 §3.10) -------------------------------------------------------------
        uint16_t deltaRecordsPerChunkCap = 256;

        //! Returns false and writes a static reason string on the first violated rule.
        bool Validate(const char** outReason) const noexcept;
    };
} // namespace LowTide::Streaming
