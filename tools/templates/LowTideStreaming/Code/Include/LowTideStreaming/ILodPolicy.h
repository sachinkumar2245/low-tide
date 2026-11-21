/*
 * LowTideStreaming - ILodPolicy.h
 *
 * LOD / HLOD selection abstraction for content the streaming gem renders itself (bulk path,
 * HLOD T1/T2). Entity-based meshes use Atom's own LOD selection, configured to the same
 * thresholds via asset settings (04 §3.7).
 *
 * DEPENDENCIES: C++17 standard library only.
 *
 * THREADING CONTRACT:
 *   Implementations must be stateless or immutable after construction: all methods are const
 *   and may be called concurrently from job workers (e.g. parallel bulk-instance LOD passes).
 *
 * INVARIANTS:
 *   L1 SelectLod applies hysteresis: switching to a finer LOD requires screenSize >=
 *      threshold * lodHysteresisFactor (04 §3.7).
 *   L2 SelectShadowLod(r) >= r (shadows never use a finer LOD than the render LOD).
 *   L3 SelectHlodMode yields exactly one representation per chunk (no double draw):
 *      Full xor Hlod1 xor Hlod2Cluster xor None.
 */
#pragma once

#include <cstdint>

namespace LowTide::Streaming
{
    //! Asset classes with distinct LOD tables (04 §3.7).
    enum class AssetClass : uint8_t
    {
        BuildingKit = 0,
        BuildingUnique,
        Prop,
        VehicleHero,
        VehicleTraffic,
        Character,
        TerrainTile,
        Tree,
        GroundCover,
        Count
    };

    constexpr uint8_t LodCulled = 0xFF;   //!< SelectLod result meaning "do not draw"

    struct LodQuery
    {
        AssetClass assetClass = AssetClass::Prop;
        float screenSize = 0.0f;          //!< projected bounding-sphere diameter / screen height
        float distanceMeters = 0.0f;      //!< used by distance-driven classes (TerrainTile)
        uint8_t currentLod = 0;           //!< for hysteresis; LodCulled if currently culled
        uint8_t lodCount = 1;             //!< authored LOD count for this asset
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

        //! Returns LOD index in [0, lodCount) or LodCulled. See L1.
        virtual uint8_t SelectLod(const LodQuery& query) const = 0;

        //! Returns shadow LOD for a given render LOD, or LodCulled for "no shadow". See L2.
        virtual uint8_t SelectShadowLod(AssetClass assetClass, uint8_t renderLod, float distanceMeters) const = 0;

        //! See L3.
        virtual HlodMode SelectHlodMode(const HlodQuery& query) const = 0;
    };
} // namespace LowTide::Streaming
