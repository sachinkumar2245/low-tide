/*
 * LowTideStreaming - StreamingSystemComponent.h  (PRIVATE header: Source/Engine, not Include/)
 *
 * System component that hosts the streaming system: owns the StreamingSystem implementation
 * (pimpl), handles ChunkStreamingRequestBus, registers AZ::Interface<ChunkStreamingRequests>,
 * and drives the per-frame scheduler from TickBus.
 *
 * VERIFIED against O3DE 26.10 source (docs/06 V-43, V-44, V-45):
 *   - Component macros: the DefaultGem template uses AZ_COMPONENT_DECL(Class) in the header and
 *     AZ_COMPONENT_IMPL(Class, "DisplayName", TypeId) in the .cpp (AZ_COMPONENT also still valid).
 *     TypeId: {A66E62C0-826D-4340-8D89-6B0DCEB7E8CA}.
 *   - Service functions take AZ::ComponentDescriptor::DependencyArrayType&; names via AZ_CRC_CE.
 *   - TickBus::Handler: void OnTick(float, AZ::ScriptTimePoint); int GetTickOrder().
 *   - Engine order: TICK_INPUT 75, TICK_GAME 80 (SpawnableSystemComponent::ProcessQueue),
 *     TICK_PHYSICS_SYSTEM 200 (PhysX Simulate, blocking on main thread).
 *
 * THREADING CONTRACT:
 *   Activate/Deactivate/OnTick/bus handlers: main thread only.
 *   Post-load TaskGraph jobs are owned by the pimpl'd StreamingSystem and are cancelled + waited
 *   for inside Deactivate before any memory is released.
 *
 * LIFETIME:
 *   Activate:   validate config (refuse to activate on failure, incl. r_meshInstancingEnabled when
 *               config.requireMeshInstancing), reserve ChunkBlockPool and fixed pools, register
 *               AZ::Interface, connect buses. NO chunk loads until the first SetStreamingFocus.
 *   Deactivate: stop issuing batches, cancel post-load jobs, synchronously despawn / release /
 *               remove every layer (unbudgeted), run leak verification, unregister, disconnect,
 *               release pools.
 *
 * TICK ORDER: config.streamingTickOrder (default 78): after input, before TICK_GAME so this
 *             frame's spawn batches are processed this frame, and before physics so newly added
 *             static bodies take part in this frame's step. Focus is consumed from the previous
 *             frame's SetStreamingFocus (one-frame latency, absorbed by radii).
 */
#pragma once

#include <LowTideStreaming/ChunkStreamingBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace LowTide::Streaming
{
    class StreamingSystem;   //!< implementation (Source/Engine/StreamingSystem.h), pimpl

    class StreamingSystemComponent
        : public AZ::Component
        , protected ChunkStreamingRequestBus::Handler
        , protected AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT_DECL(StreamingSystemComponent);

        StreamingSystemComponent();
        ~StreamingSystemComponent() override;

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

    protected:
        // AZ::Component
        void Init() override;
        void Activate() override;
        void Deactivate() override;

        // AZ::TickBus::Handler
        void OnTick(float deltaTimeSeconds, AZ::ScriptTimePoint time) override;
        int GetTickOrder() override;

        // ChunkStreamingRequestBus::Handler (contracts documented in ChunkStreamingBus.h)
        void SetStreamingFocus(const StreamingFocus& focus) override;
        void SetPredictedRoute(const AZ::Vector2* points, uint32_t pointCount) override;
        PinId RequestPin(const AZ::Vector3& center, float radiusMeters, uint8_t layerMask, PinReason reason) override;
        PinId RequestCorridorPin(const AZ::Vector2* points, uint32_t pointCount, float halfWidthMeters, uint8_t layerMask) override;
        void ReleasePin(PinId pin) override;
        SyncLoadTicket BeginSynchronousLoad(const AZ::Vector3& center) override;
        float GetSynchronousLoadProgress(SyncLoadTicket ticket) const override;
        ChunkHandle GetChunkHandle(ChunkCoord coord, ChunkLayer layer) const override;
        ChunkState GetLayerState(const ChunkHandle& handle) const override;
        bool IsCollisionReady(const AZ::Vector3& position) const override;
        ChunkCoord WorldToChunk(const AZ::Vector3& position) const override;
        DegradationLevel GetDegradationLevel() const override;
        float GetSpeedCapMps() const override;
        void RequestVariantReevaluation() override;
        IChunkDeltaStore& GetDeltaStore() override;
        const ChunkStreamingConfig& GetConfig() const override;
        const StreamingFrameStats& GetFrameStats() const override;

    private:
        StreamingSystemComponent(const StreamingSystemComponent&) = delete;
        StreamingSystemComponent& operator=(const StreamingSystemComponent&) = delete;

        AZStd::unique_ptr<StreamingSystem> m_system;   //!< null outside Activate..Deactivate
    };
} // namespace LowTide::Streaming
