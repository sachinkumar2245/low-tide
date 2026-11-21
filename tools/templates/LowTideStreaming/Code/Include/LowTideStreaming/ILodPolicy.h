/*
 * LowTideStreaming - ILodPolicy.h
 *
 * LOD / HLOD policy for content the streaming gem renders through Atom mesh handles.
 *
 * ENGINE MODEL (verified against O3DE 26.10 source, docs/06 V-55, V-56, V-58):
 *   Atom selects LODs PER MESH via RPI::Cullable::LodConfiguration
 *   {m_lodType: Default|ScreenCoverage|SpecificLod, m_lodOverride, m_minimumScreenCoverage,
 *    m_qualityDecayRate}. There are no per-LOD thresholds, no hysteresis, no cross-fade, and no
 *   separate shadow LOD. Therefore this policy does NOT pick LODs per frame; it supplies
 *   per-class LodConfiguration values (applied once at AcquireMesh / SetMeshLodConfiguration),
 *   per-ring forced LODs for terrain tiles, and the HLOD representation choice we own.
 *
 * DEPENDENCIES: C++17 standard library only (engine glue converts LodSettings to
 *               AZ::RPI::Cullable::LodConfiguration).
 *
 * THREADING CONTRACT:
 *   Implementations are immutable after construction; all methods are const and may be called
 *   from any thread. Applying results to mesh handles is main-thread only (MeshFeatureProcessor
 *   has no locks).
 *
 * INVARIANTS:
 *   L1 GetLodSettings is a pure function of AssetClass (stable for the session).
 *   L2 SelectTerrainLod is monotonic non-decreasing in distance.
 *   L3 SelectHlodMode yields exactly one representation per chunk:
 *      Full xor Hlod1 xor Hlod2Cluster xor None (no double draw).
 */
#pragma once

#include <cstdint>

namespace LowTide::Streaming
{
    //! Asset classes with distinct LOD settings (04 §3.7).
    enum class AssetClass : uint8_t
    {
        MergedKitSection = 0,
        BuildingUnique,
        Prop,
        VehicleHero,
        VehicleTraffic,
        Character,
        TerrainTile,
        Tree,
        GroundCover,
        Hlod,
        Count
    };

    //! Mirrors RPI::Cullable::LodType.
    enum class LodMode : uint8_t
    {
        EngineDefault = 0,   //!< LodType::Default
        ScreenCoverage,      //!< LodType::ScreenCoverage (uses the two parameters below)
        Forced               //!< LodType::SpecificLod (uses forcedLod)
    };

    //! Mirrors RPI::Cullable::LodConfiguration fields.
    struct LodSettings
    {
        LodMode mode = LodMode::ScreenCoverage;
        uint8_t forcedLod = 0;                    //!< used when mode == Forced
        float minimumScreenCoverage = 1.0f / 1080.0f; //!< below this the mesh is culled; 0 = never
        float qualityDecayRate = 0.5f;            //!< engine-derived per-LOD coverage spacing
    };

    enum class HlodMode : uint8_t
    {
        Full = 0,       //!< StaticVisual layer Active: draw real content
        Hlod1,          //!< per-chunk proxy
        Hlod2Cluster,   //!< 4x4 cluster proxy (drawn once per cluster)
        None            //!< beyond all ranges
    };

    struct HlodQuery
    {
        float chunkDistanceMeters = 0.0f;
        float clusterDistanceMeters = 0.0f;   //!< nearest point of the chunk's 4x4 cluster
        bool staticVisualActive = false;
        bool hlod1Resident = false;
    };

    class ILodPolicy
    {
    public:
        virtual ~ILodPolicy() = default;

        //! See L1. Applied when acquiring a handle of this class.
        virtual LodSettings GetLodSettings(AssetClass assetClass) const = 0;

        //! Forced LOD index for a terrain tile at this distance (ring LOD), see L2.
        //! The streamer calls SetMeshLodConfiguration only when the result changes.
        virtual uint8_t SelectTerrainLod(float distanceMeters, uint8_t lodCount) const = 0;

        //! See L3.
        virtual HlodMode SelectHlodMode(const HlodQuery& query) const = 0;
    };
} // namespace LowTide::Streaming
