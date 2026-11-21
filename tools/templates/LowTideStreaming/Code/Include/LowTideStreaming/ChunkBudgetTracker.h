/*
 * LowTideStreaming - ChunkBudgetTracker.h
 *
 * Per (chunk, layer) resource counters + per-frame streaming statistics.
 * Purpose: (1) prove every unload returns everything it acquired (leak detection, 04 §3.5),
 *          (2) feed the streaming HUD and benchmark CSV (docs/05), (3) check runtime numbers
 *              against the offline budget report (04 §3.8).
 *
 * DEPENDENCIES: C++17 standard library only.
 *
 * THREADING CONTRACT:
 *   - Counter increments (On* methods) may be called from any thread; they use relaxed atomics
 *     on 64-byte-padded slots (no false sharing between hot counters).
 *   - VerifyReleased, BeginFrame, EndFrame, GetFrameStats: main thread only.
 *   - Snapshot reads for HUD: main thread; values are eventually consistent (relaxed).
 *
 * INVARIANTS:
 *   B1 For a layer reaching Unloaded: spawned == despawned, acquired == released, added == removed,
 *      assetRefs == 0, arenaBlocks == 0. Violation -> LeakReport (assert in debug, log in profile).
 *   B2 Counters are reset when a layer's generation changes (new residency cycle).
 *   B3 No heap allocation after construction (fixed arrays sized from config).
 */
#pragma once

#include <LowTideStreaming/ChunkState.h>
#include <LowTideStreaming/ChunkTypes.h>

#include <cstddef>
#include <cstdint>

namespace LowTide::Streaming
{
    enum class ResourceKind : uint8_t
    {
        Entity = 0,
        MeshHandle,
        SimulatedBody,      //!< AzPhysics simulated body (entity-less static)
        AssetRef,
        ArenaBlock,
        Count
    };

    constexpr uint8_t ResourceKindCount = static_cast<uint8_t>(ResourceKind::Count);

    //! Result of VerifyReleased. outstanding[k] > 0 means kind k leaked.
    struct LeakReport
    {
        ChunkHandle handle;
        int32_t outstanding[ResourceKindCount] = {};
        bool HasLeak() const noexcept;
    };

    //! Per-frame aggregate published to HUD/CSV (docs/05 instrumentation).
    struct StreamingFrameStats
    {
        uint64_t frameIndex = 0;
        uint16_t layersByState[ChunkLayerCount][ChunkStateCount] = {};
        uint8_t layersInFlight = 0;
        uint64_t bytesInFlight = 0;
        uint16_t queueLength = 0;
        float schedulerMs = 0.0f;
        float instantiateMs = 0.0f;
        float deactivateMs = 0.0f;
        float minGameplaySlackSeconds = 0.0f;      //!< min over wanted G layers of (ETA - T_ready_est)
        float nearestUnreadyGameplayMeters = 0.0f;
        DegradationLevel degradation = DegradationLevel::L0_Normal;
        float speedCapMps = 0.0f;                  //!< 0 = no cap active
        uint32_t arenaBlocksInUse = 0;
        uint64_t loadedInactiveCacheBytes = 0;
        uint32_t entitiesLive = 0;
        uint32_t meshHandlesLive = 0;              //!< vs config.maxMeshHandlesResident
        uint32_t simulatedBodiesLive = 0;          //!< entity-less static bodies
        float spawnProcessingMsEma = 0.0f;         //!< measured engine cost of our spawn batches at TICK_GAME
        uint32_t failedLayers = 0;
        uint32_t transitionsThisFrame = 0;
    };

    class ChunkBudgetTracker
    {
    public:
        //! Capacity = maxChunks * ChunkLayerCount slots, allocated once here.
        explicit ChunkBudgetTracker(uint32_t maxChunks);
        ~ChunkBudgetTracker();

        ChunkBudgetTracker(const ChunkBudgetTracker&) = delete;
        ChunkBudgetTracker& operator=(const ChunkBudgetTracker&) = delete;

        // Residency cycle boundary (main thread): resets the slot for the new generation (B2).
        void OnNewGeneration(const ChunkHandle& handle) noexcept;

        // Resource accounting (any thread). count may be > 1 for batched operations.
        void OnAcquired(const ChunkHandle& handle, ResourceKind kind, uint32_t count = 1) noexcept;
        void OnReleased(const ChunkHandle& handle, ResourceKind kind, uint32_t count = 1) noexcept;

        //! Main thread, at Unloading -> Unloaded. See B1.
        LeakReport VerifyReleased(const ChunkHandle& handle) const noexcept;

        // Frame statistics (main thread).
        void BeginFrame(uint64_t frameIndex) noexcept;
        StreamingFrameStats& EditFrameStats() noexcept;          //!< writable during the frame
        void EndFrame() noexcept;                                 //!< publishes to the read slot
        const StreamingFrameStats& GetFrameStats() const noexcept; //!< last completed frame
    };
} // namespace LowTide::Streaming
