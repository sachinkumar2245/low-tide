/*
 * LowTideStreaming - ChunkStreamingBus.h
 *
 * Public engine-facing API of the streaming system:
 *   ChunkStreamingRequests      (the brief's "IChunkStreamingRequests"; named per O3DE's
 *                                <Name>Requests / <Name>RequestBus convention, docs/03 §2.4)
 *   ChunkStreamingNotifications (lifecycle events for gameplay, missions, HUD, save system)
 *
 * VERIFIED against O3DE 26.10 source (docs/06 V-42, V-45):
 *   - AZ::EBusTraits with static constexpr HandlerPolicy / AddressPolicy
 *     (EBusHandlerPolicy::{Single, Multiple, MultipleAndOrdered}; EBusAddressPolicy::{Single, ById,
 *     ByIdAndOrdered}); AZ::EBus<Interface> alias; include paths exist.
 *   - The DefaultGem template ALSO registers system interfaces with AZ::Interface<T>
 *     (Register/Unregister return InterfaceRegisterOutcome; Interface<T>::Registrar RAII helper).
 *     Decision: StreamingSystemComponent registers AZ::Interface<ChunkStreamingRequests> in
 *     Activate as well, for direct calls from C++ (no bus dispatch); the bus stays for script/
 *     tool access. Both obey the same main-thread contract below.
 *
 * THREADING CONTRACT (both buses):
 *   MAIN THREAD ONLY. Buses use the default (non-thread-safe) mutex policy. Calls from job or
 *   I/O threads are contract violations and are asserted in debug/profile builds.
 *   Notifications are dispatched synchronously on the main thread during
 *   StreamingSystemComponent::OnTick (never from inside a worker).
 *
 * REFERENCE LIFETIME RULE:
 *   Handlers must drop any references (EntityIds, handles) into a layer in
 *   OnLayerDeactivating; after that notification returns, the layer's entities may be despawned
 *   at any slice boundary.
 */
#pragma once

#include <LowTideStreaming/ChunkBudgetTracker.h>
#include <LowTideStreaming/ChunkState.h>
#include <LowTideStreaming/ChunkTypes.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector3.h>

namespace LowTide::Streaming
{
    class IChunkDeltaStore;
    struct ChunkStreamingConfig;

    //! Streaming focus supplied once per frame by the game (player pawn or vehicle).
    struct StreamingFocus
    {
        AZ::Vector3 position;               //!< world meters
        AZ::Vector3 velocity;               //!< m/s
        AZ::Vector3 cameraPosition;         //!< used for S/D/H1 layers when ahead of the pawn
        bool isInVehicle = false;
        bool isBoat = false;
    };

    //! Ticket for a blocking (behind-fade) load. 0 == invalid.
    struct SyncLoadTicket { uint32_t value = 0; };

    // =========================================================================================
    // Requests
    // =========================================================================================
    class ChunkStreamingRequests
        : public AZ::EBusTraits
    {
    public:
        // Verified: O3DE 26.10 EBusTraits (docs/06 V-42).
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual ~ChunkStreamingRequests() = default;

        // --- Focus / prediction --------------------------------------------------------------
        //! Must be called exactly once per frame before the streaming tick (tick order: see
        //! StreamingSystemComponent::GetTickOrder). Missing focus = streaming holds the last set.
        virtual void SetStreamingFocus(const StreamingFocus& focus) = 0;

        //! Optional route for road-prediction priority (GPS waypoint). Pass nullptr/0 to clear.
        //! Points are world XY meters; copied internally (no reference retained).
        virtual void SetPredictedRoute(const AZ::Vector2* points, uint32_t pointCount) = 0;

        // --- Pins ---------------------------------------------------------------------------
        //! Pins every chunk whose AABB intersects the circle (center, radius) for the given layers
        //! (bitmask of 1 << ChunkLayer). Pinned layers load at top priority and never unload while
        //! pinned. Returns invalid PinId if the pin table (fixed capacity) is full.
        virtual PinId RequestPin(const AZ::Vector3& center, float radiusMeters, uint8_t layerMask, PinReason reason) = 0;

        //! Pins a precomputed corridor (scripted path / race course). Points copied internally.
        virtual PinId RequestCorridorPin(const AZ::Vector2* points, uint32_t pointCount, float halfWidthMeters, uint8_t layerMask) = 0;

        //! Releasing an unknown/invalid PinId is a no-op (logged in debug).
        virtual void ReleasePin(PinId pin) = 0;

        // --- Synchronous loads (save-load, mission restart, vignette exit) -------------------
        //! Caller must have faded out. Streaming ignores frame budgets until complete.
        virtual SyncLoadTicket BeginSynchronousLoad(const AZ::Vector3& center) = 0;
        //! Progress in [0,1]; 1 when every required G and S layer in radius is Active.
        virtual float GetSynchronousLoadProgress(SyncLoadTicket ticket) const = 0;

        // --- Queries ------------------------------------------------------------------------
        //! Handle for the current residency of (coord, layer); invalid handle if Unloaded.
        virtual ChunkHandle GetChunkHandle(ChunkCoord coord, ChunkLayer layer) const = 0;

        //! Unloaded if the handle is stale (generation mismatch) - never a dangling read.
        virtual ChunkState GetLayerState(const ChunkHandle& handle) const = 0;

        //! True if a collidable representation (Active G layer or coarse proxy) exists at position.
        virtual bool IsCollisionReady(const AZ::Vector3& position) const = 0;

        virtual ChunkCoord WorldToChunk(const AZ::Vector3& position) const = 0;

        // --- Degradation hooks (AD-06) --------------------------------------------------------
        virtual DegradationLevel GetDegradationLevel() const = 0;
        //! 0 = no cap. Vehicle systems read this each frame and apply a smooth drive-force limit.
        virtual float GetSpeedCapMps() const = 0;

        // --- Global-state changes (01 §1.2 rule 5) ------------------------------------------
        //! Called by global state owners (world phase, business status, faction influence) after a
        //! change. Re-evaluates AD-09 variant groups IN PLACE on Active layers (time-sliced);
        //! never triggers a reload.
        virtual void RequestVariantReevaluation() = 0;

        // --- Access ---------------------------------------------------------------------------
        virtual IChunkDeltaStore& GetDeltaStore() = 0;
        virtual const ChunkStreamingConfig& GetConfig() const = 0;
        virtual const StreamingFrameStats& GetFrameStats() const = 0;
    };

    using ChunkStreamingRequestBus = AZ::EBus<ChunkStreamingRequests>;

    // =========================================================================================
    // Notifications
    // =========================================================================================
    class ChunkStreamingNotifications
        : public AZ::EBusTraits
    {
    public:
        // Verified: O3DE 26.10 EBusTraits (docs/06 V-42).
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual ~ChunkStreamingNotifications() = default;

        //! Every state transition (invariant I5). High frequency: handlers must be O(1).
        virtual void OnLayerStateChanged(const ChunkHandle& /*handle*/, ChunkState /*from*/, ChunkState /*to*/) {}

        //! Layer fully Active: entities spawned, deltas applied, variants evaluated.
        virtual void OnLayerActivated(const ChunkHandle& /*handle*/) {}

        //! Last chance to drop references and record deltas before despawn begins.
        virtual void OnLayerDeactivating(const ChunkHandle& /*handle*/) {}

        virtual void OnLayerFailed(const ChunkHandle& /*handle*/, FailureStage /*stage*/) {}

        virtual void OnDegradationLevelChanged(DegradationLevel /*previous*/, DegradationLevel /*current*/) {}

        virtual void OnSynchronousLoadComplete(SyncLoadTicket /*ticket*/) {}
    };

    using ChunkStreamingNotificationBus = AZ::EBus<ChunkStreamingNotifications>;
} // namespace LowTide::Streaming
