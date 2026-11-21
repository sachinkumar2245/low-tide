/*
 * LowTideStreaming - ICullingPolicy.h
 *
 * The culling decisions this gem owns (04 §3.6):
 *   (e) interior/portal visibility            - runtime, per frame
 *   (c) occluder-plane selection              - runtime, per frame
 *   (f) shadow-caster eligibility             - BUILD TIME (used by the .ltchunk builder and the
 *                                               LOD validator to choose *_NoShadow materials)
 *
 * ENGINE FACTS (O3DE 26.10 source, docs/06): frustum culling is Atom CullingScene over the
 * AzFramework octree (V-53); only OcclusionCullingPlane quads occlude, rasterized by Masked
 * Occlusion Culling (V-54); shadow opt-out is the material property castShadows, because
 * SetDrawItemEnabled is a no-op with mesh instancing enabled (V-52, V-56). There is no
 * per-instance, per-cascade shadow control at runtime, so (f) is not a runtime decision.
 *
 * DEPENDENCIES: C++17 standard library only.
 *
 * THREADING CONTRACT:
 *   All methods const; implementations immutable after construction; callable from any thread.
 *   Results are applied to engine objects by the caller on the main thread.
 *
 * INVARIANTS:
 *   C1 IsInteriorVisible returns true whenever cameraInsideCell is true.
 *   C2 SelectOccluders writes at most maxOut indices, ordered by descending projected area,
 *      and never allocates.
 *   C3 ShouldCastShadow is false for GroundCover, Hlod, and Detail-layer content.
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
        bool portalInFrustum = false;     //!< computed by caller with the engine view frustum
        float portalDistanceMeters = 0.0f;
        float portalFacingDot = 0.0f;     //!< dot(cameraForward, -portalNormal); > 0 = facing
    };

    struct OccluderCandidate
    {
        uint32_t occluderIndex = 0;       //!< index into the chunk's occluder-plane table
        float projectedAreaFraction = 0.0f;
        float distanceMeters = 0.0f;
    };

    //! Build-time description of an authored object (from .ltmeta).
    struct ShadowCasterQuery
    {
        AssetClass assetClass = AssetClass::Prop;
        ChunkLayer layer = ChunkLayer::StaticVisual;
        float boundsDiameterMeters = 0.0f;
    };

    class ICullingPolicy
    {
    public:
        virtual ~ICullingPolicy() = default;

        //! Runtime. See C1. Interior render group visible iff true.
        virtual bool IsInteriorVisible(const PortalQuery& query) const = 0;

        //! Runtime. See C2. Returns number of indices written to outIndices.
        virtual uint32_t SelectOccluders(
            const OccluderCandidate* candidates, uint32_t candidateCount,
            uint32_t* outIndices, uint32_t maxOut) const = 0;

        //! BUILD TIME. See C3. false => the builder assigns the *_NoShadow material variant.
        virtual bool ShouldCastShadow(const ShadowCasterQuery& query) const = 0;
    };
} // namespace LowTide::Streaming
