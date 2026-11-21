/*
 * LowTideStreaming - ChunkStreamingConfig.h
 *
 * Every streaming tunable from docs/04 as one value struct with defaults.
 * All defaults are TUNABLE STARTING VALUES [T] to be validated by the testbed (docs/05).
 * Runtime source of truth: Registry/DT_StreamingConfig_Default.setreg (loaded by engine glue,
 * which overrides these defaults). Units are encoded in member names.
 *
 * DEPENDENCIES: C++17 standard library only. Reflection/serialization is provided by the engine
 * glue (Source/Engine), not in this header. Planned TypeId for reflection:
 * {1FCB43A2-B89D-4CC2-9F2A-2B9C0D8C08A5} [VERIFY AZ_TYPE_INFO_SPECIALIZE usage for external types].
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
        float schedulerBudgetMs = 0.2f;
        float instantiateBudgetMs = 1.5f;
        float deactivateBudgetMs = 0.3f;
        float instantiateBudgetUnderDeficitMaxMs = 4.0f;
        float frameTargetMs = 16.6f;
        float workItemMaxMs = 0.25f;                 //!< work items pre-split to at most this cost
        uint16_t workItemMaxEntities = 10;
        uint16_t workItemMaxBulkInstances = 250;

        // --- I/O and jobs (04 §3.4) --------------------------------------------------------------
        uint8_t maxLayersInFlight = 4;
        size_t maxBytesInFlight = 64ull * 1024 * 1024;
        uint8_t maxConcurrentDeserializeJobs = 2;
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
        uint8_t hlodClusterDim = 4;                  //!< T2 cluster = 4x4 chunks
        float hlodT2SwitchMeters = 1000.0f;
        float lodHysteresisFactor = 1.1f;
        uint8_t maxActiveOccluders = 32;
        uint8_t maxShadowedLocalLightsGlobal = 8;
        float smallPropShadowCutoffMeters = 40.0f;
        float interiorPortalMaxMeters = 64.0f;

        // --- Persistence (04 §3.10) -------------------------------------------------------------
        uint16_t deltaRecordsPerChunkCap = 256;

        //! Returns false and writes a static reason string on the first violated rule.
        bool Validate(const char** outReason) const noexcept;
    };
} // namespace LowTide::Streaming
