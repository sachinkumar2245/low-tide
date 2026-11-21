/*
 * LowTideStreaming - StreamingSystemComponent.h  (PRIVATE header: Source/Engine, not Include/)
 *
 * System component that hosts the streaming system: owns the StreamingSystem implementation
 * (pimpl), handles ChunkStreamingRequestBus, and drives the per-frame scheduler from TickBus.
 *
 * [VERIFY] against O3DE 26.05 (Code/Framework/AzCore/AzCore/Component/Component.h, TickBus.h,
 *          and the DefaultGem template's <Gem>SystemComponent.h):
 *   - AZ_COMPONENT vs AZ_COMPONENT_DECL/AZ_COMPONENT_IMPL macro pair
 *   - service function signatures (DependencyArrayType)
 *   - TickBus::Handler OnTick(float, AZ::ScriptTimePoint) and GetTickOrder() / AZ::TICK_* constants
 *   - AZ_CRC_CE for service names
 *
 * THREADING CONTRACT:
 *   Activate/Deactivate/OnTick/bus handlers: main thread only.
 *   Worker jobs are owned by the pimpl'd StreamingSystem and are drained (cancelled + joined)
 *   inside Deactivate before any memory is released.
 *
 * LIFETIME:
 *   Activate:   validate config (refuse to activate on failure), reserve ChunkBlockPool and all
 *               fixed pools, connect buses. NO chunk loads until the first SetStreamingFocus.
 *   Deactivate: cancel all in-flight work, synchronously deactivate + unload every layer
 *               (unbudgeted), run leak verification for every layer, disconnect buses, release pools.
 *
 * TICK ORDER: after gameplay sets focus (game tick), before physics simulate, so newly Active
 *             collision is present for the same frame's step. Exact value chosen in P2 [VERIFY
 *             tick order constants and physics system tick placement].
 */
#pragma once

#include <LowTideStreaming/ChunkStreamingBus.h>

#include <AzCore/Component/Component.h>   // [VERIFY include path]
#include <AzCore/Component/TickBus.h>     // [VERIFY include path]
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
        // TypeId generated for this project. [VERIFY macro form]
        AZ_COMPONENT(StreamingSystemComponent, "{A66E62C0-826D-4340-8D89-6B0DCEB7E8CA}");

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
