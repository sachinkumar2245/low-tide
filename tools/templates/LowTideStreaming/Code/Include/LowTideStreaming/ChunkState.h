/*
 * LowTideStreaming - ChunkState.h
 *
 * Per (chunk, layer) lifecycle states and the transition contract.
 *
 * DEPENDENCIES: C++17 standard library only.
 *
 * THREADING CONTRACT:
 *   The enum and the pure query functions are thread-agnostic.
 *   STATE OWNERSHIP: only the main-thread scheduler ever writes a layer's state. Workers and
 *   I/O callbacks report results through the completion queue; they never transition state.
 *
 * DESIGN REFERENCE: docs/04_World_Streaming_Design.md §3.3 (diagram, timeouts, hysteresis).
 */
#pragma once

#include <cstdint>

namespace LowTide::Streaming
{
    enum class ChunkState : uint8_t
    {
        Unloaded = 0,     //!< descriptor only; no memory held
        Queued,           //!< wanted; waiting in priority queue (main)
        IOPending,        //!< Asset<T> loads in flight; parse runs in AssetManager LoadAssetJob;
                          //!< ready notified on main via AssetManager::DispatchEvents
        Deserializing,    //!< post-load TaskGraph job: variants, spawn batch lists, shape creation
        LoadedInactive,   //!< data resident (warm cache), nothing in the world (engine has no
                          //!< "spawn inactive"; this state is pre-spawn by design)
        Instantiating,    //!< main thread, sliced: SpawnEntities(indices) batches (<= 1 in flight
                          //!< per ticket), AcquireMesh batches, AddSimulatedBodies batches
        Active,           //!< in world, simulated, visible
        Deactivating,     //!< main thread, sliced: flush deltas, DespawnEntity / ReleaseMesh /
                          //!< RemoveSimulatedBodies batches
        Unloading,        //!< release asset refs, return arena blocks
        Failed,           //!< retries exhausted; fallback (coarse proxy) engaged for Gameplay
        Count
    };

    constexpr uint8_t ChunkStateCount = static_cast<uint8_t>(ChunkState::Count);

    /*
     * ALLOWED TRANSITIONS (exhaustive; anything else is a contract violation and asserts):
     *
     *   Unloaded       -> Queued
     *   Queued         -> IOPending | Unloaded (cancel)
     *   IOPending      -> Deserializing | Unloading (cancel) | Failed (timeout/error after retries)
     *   Deserializing  -> LoadedInactive | Unloading (cancel) | Failed (validation)
     *   LoadedInactive -> Instantiating | Unloading (evict)
     *   Instantiating  -> Active | Deactivating (abort at slice boundary)
     *   Active         -> Deactivating            (never while pinned)
     *   Deactivating   -> LoadedInactive
     *   Unloading      -> Unloaded
     *   Failed         -> Unloading               (retry path re-enters via Unloaded -> Queued)
     *
     * INVARIANTS:
     *   I1  Active is reachable only from Instantiating; Instantiating only from LoadedInactive.
     *   I2  A pinned layer in Queued..Active never takes a cancel/deactivate/evict edge.
     *   I3  Generation increments exactly once per Unloaded -> Queued -> IOPending cycle (on IOPending entry).
     *   I4  Interior and Detail layers may enter Instantiating only if the same chunk's Gameplay layer is Active.
     *   I5  Every transition is observable via ChunkStreamingNotificationBus::OnLayerStateChanged (main thread).
     *   I6  At most one spawn batch is queued per EntitySpawnTicket at any time: queued spawns cannot be
     *       cancelled in O3DE 26.10 (docs/06 V-37), so this bounds cancellation latency to one frame.
     */

    //! True iff (from -> to) appears in the table above. Pure, constant-time.
    bool IsValidTransition(ChunkState from, ChunkState to) noexcept;

    //! True for states that hold engine objects in the world (Instantiating, Active, Deactivating).
    bool IsInWorld(ChunkState state) noexcept;

    //! True for states with outstanding asynchronous work that can be cancelled (Queued, IOPending, Deserializing).
    bool IsCancellable(ChunkState state) noexcept;

    //! True for states that hold arena memory (Deserializing .. Deactivating).
    bool HoldsArena(ChunkState state) noexcept;

    //! Stage that failed, reported with OnLayerFailed.
    enum class FailureStage : uint8_t
    {
        IoTimeout = 0,
        IoError,
        Validation,
        InstantiateTimeout   //!< reported but non-fatal (layer continues), see 04 §3.3
    };

    //! Debug name; returns a static string. Never null.
    const char* ToString(ChunkState state) noexcept;
    const char* ToString(FailureStage stage) noexcept;
} // namespace LowTide::Streaming
