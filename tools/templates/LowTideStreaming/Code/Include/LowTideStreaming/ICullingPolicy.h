/*
 * LowTideStreaming - ICullingPolicy.h
 *
 * The culling decisions this gem owns (04 §3.6): interior/portal visibility (e), occluder
 * selection for engine occlusion planes (c), and shadow-caster eligibility (f).
 * Frustum culling (b) and the occlusion rasterization itself are provided by Atom [VERIFY]
 * and are NOT abstracted here.
 *
 * DEPENDENCIES: C++17 standard library only.
 *
 * THREADING CONTRACT:
 *   All methods const and safe to call concurrently (implementations immutable after construction).
 *   Results are applied to engine objects by the caller on the main thread.
 *
 * INVARIANTS:
 *   C1 IsInteriorVisible returns true whenever cameraInsideCell is true.
 *   C2 SelectOccluders writes at most maxOut indices, ordered by descending projected area,
 *      and never allocates.
 *   C3 ShouldCastShadow is false for AssetClass::GroundCover and for Detail-layer content.
 */
#pragma once

#include <LowTideStreaming/ChunkTypes.h>
#include <LowTideStreaming/ILodPolicy.h>

#include <cstdint>

namespace LowTide::Streaming
{
    struct PortalQuery
    {
        bool cameraInsideCell = false;
        bool portalInFrustum = false;     //!< computed by caller with engine frustum
        float portalDistanceMeters = 0.0f;
        float portalFacingDot = 0.0f;     //!< dot(cameraForward, -portalNormal); > 0 = facing
    };

    struct OccluderCandidate
    {
        uint32_t occluderIndex = 0;       //!< index into the chunk's occluder table
        float projectedAreaFraction = 0.0f;
        float distanceMeters = 0.0f;
    };

    struct ShadowCasterQuery
    {
        AssetClass assetClass = AssetClass::Prop;
        ChunkLayer layer = ChunkLayer::StaticVisual;
        float boundsDiameterMeters = 0.0f;
        float distanceMeters = 0.0f;
        bool isHlod = false;
        uint8_t hlodTier = 0;             //!< 1 or 2 when isHlod
        uint8_t cascadeIndex = 0;
        uint8_t cascadeCount = 4;
    };

    class ICullingPolicy
    {
    public:
        virtual ~ICullingPolicy() = default;

        //! See C1. Interior render group visible iff true.
        virtual bool IsInteriorVisible(const PortalQuery& query) const = 0;

        //! See C2. Returns number of indices written to outIndices.
        virtual uint32_t SelectOccluders(
            const OccluderCandidate* candidates, uint32_t candidateCount,
            uint32_t* outIndices, uint32_t maxOut) const = 0;

        //! See C3. Per-cascade decision (H1 last cascade only; H2 never).
        virtual bool ShouldCastShadow(const ShadowCasterQuery& query) const = 0;
    };
} // namespace LowTide::Streaming
