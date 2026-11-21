/*
 * LowTideStreaming - ChunkTypes.h
 *
 * Core identifiers for the world grid: coordinates, ids, layers, generational handles,
 * stable authored ids, and minimal math PODs.
 *
 * DEPENDENCIES: C++17 standard library only (no AzCore). Compiles in tests/core_standalone.
 *
 * THREADING CONTRACT:
 *   All types are trivially-copyable value types. Safe to copy/read on any thread.
 *   Resolving a ChunkHandle against live state is NOT done here; it is a main-thread-only
 *   operation on ChunkStreamingRequestBus (see ChunkStreamingBus.h).
 *
 * DESIGN REFERENCE: docs/04_World_Streaming_Design.md §3.1, §3.2, §3.5 (ownership rules).
 */
#pragma once

#include <cstdint>

namespace LowTide::Streaming
{
    // ---------------------------------------------------------------------------------------
    // Grid constants (compile-time maxima; runtime dimensions live in ChunkStreamingConfig).
    // ---------------------------------------------------------------------------------------

    //! Hard upper bound on grid dimension per axis. Runtime grid (32x32) must be <= this.
    //! 64 * 128 m = 8 192 m: beyond the 4 096 m precision limit from origin, so runtime config
    //! validation must also enforce (dimension * chunkSize / 2) * sqrt(2) <= 4 096 m.
    constexpr uint16_t MaxGridDimension = 64;

    //! Maximum number of chunks addressable by ChunkId.
    constexpr uint32_t MaxChunkCount = uint32_t(MaxGridDimension) * MaxGridDimension; // 4 096

    // ---------------------------------------------------------------------------------------
    // Minimal math PODs (engine glue converts to/from AZ::Vector2/3).
    // ---------------------------------------------------------------------------------------
    struct Vec2f { float x = 0.0f; float y = 0.0f; };
    struct Vec3f { float x = 0.0f; float y = 0.0f; float z = 0.0f; };

    //! Axis-aligned rectangle on the XY ground plane, world meters. Invariant: min <= max per axis.
    struct AabbXY { Vec2f min; Vec2f max; };

    // ---------------------------------------------------------------------------------------
    // ChunkCoord / ChunkId
    // ---------------------------------------------------------------------------------------

    //! Integer grid coordinate. (0,0) = south-west chunk. +X east, +Y north.
    //! Invariant (when valid): 0 <= x,y < runtime grid dimension.
    struct ChunkCoord
    {
        int16_t x = -1;
        int16_t y = -1;
    };

    //! Dense chunk index: id = y * gridDimX + x. Stable for a given grid configuration.
    //! Invariant: value < MaxChunkCount, or value == InvalidChunkId.
    struct ChunkId
    {
        uint16_t value = 0xFFFF;
    };

    constexpr uint16_t InvalidChunkIdValue = 0xFFFF;

    bool IsValid(ChunkId id) noexcept;
    bool IsValid(ChunkCoord coord, uint16_t gridDimX, uint16_t gridDimY) noexcept;

    //! Pure conversions. Return InvalidChunkId / ChunkCoord{-1,-1} on out-of-range input.
    ChunkId ToChunkId(ChunkCoord coord, uint16_t gridDimX, uint16_t gridDimY) noexcept;
    ChunkCoord ToChunkCoord(ChunkId id, uint16_t gridDimX) noexcept;

    //! World-space position (meters) -> containing chunk. Grid origin offset: the world origin is
    //! the grid centre, so chunk (x,y) min corner = ((x - dimX/2) * size, (y - dimY/2) * size).
    ChunkCoord WorldToChunkCoord(Vec2f worldPos, float chunkSizeMeters, uint16_t gridDimX, uint16_t gridDimY) noexcept;
    AabbXY ChunkBounds(ChunkCoord coord, float chunkSizeMeters, uint16_t gridDimX, uint16_t gridDimY) noexcept;

    bool operator==(ChunkCoord a, ChunkCoord b) noexcept;
    bool operator!=(ChunkCoord a, ChunkCoord b) noexcept;
    bool operator==(ChunkId a, ChunkId b) noexcept;
    bool operator!=(ChunkId a, ChunkId b) noexcept;

    // ---------------------------------------------------------------------------------------
    // Layers
    // ---------------------------------------------------------------------------------------

    //! Independently streamed content layer of a chunk (04 §3.2 taxonomy).
    //! Dependency invariant: Gameplay must be Active before Interior or Detail instantiate.
    enum class ChunkLayer : uint8_t
    {
        Gameplay = 0,     //!< G: collision, interactables, triggers, near lights, audio zones
        StaticVisual,     //!< S: terrain render tile, kit geometry (bulk), road surface, probes
        Detail,           //!< D: decorative non-collidable bulk props, decals
        Interior,         //!< I: seamless interior content (per host chunk)
        Hlod1,            //!< H1: per-chunk proxy mesh + coarse collision proxy
        Count
    };

    constexpr uint8_t ChunkLayerCount = static_cast<uint8_t>(ChunkLayer::Count);

    // ---------------------------------------------------------------------------------------
    // Generational handle
    // ---------------------------------------------------------------------------------------

    //! Reference to ONE residency cycle of a (chunk, layer).
    //! - generation increments every time the layer enters IOPending from Unloaded.
    //! - generation 0 is reserved as invalid; wrap-around skips 0.
    //! - A handle whose generation != current generation resolves to "not resident".
    //! Rule (04 §3.5): code outside the gem never holds raw record pointers, only handles.
    struct ChunkHandle
    {
        ChunkId chunk;
        ChunkLayer layer = ChunkLayer::Gameplay;
        uint8_t reserved = 0;      //!< padding; must be zero
        uint16_t generation = 0;   //!< 0 == invalid
    };
    static_assert(sizeof(ChunkHandle) == 6, "ChunkHandle must stay 6 bytes (packed value type)");

    bool IsValid(const ChunkHandle& handle) noexcept;   //!< structural validity only (not liveness)
    bool operator==(const ChunkHandle& a, const ChunkHandle& b) noexcept;
    bool operator!=(const ChunkHandle& a, const ChunkHandle& b) noexcept;

    // ---------------------------------------------------------------------------------------
    // Stable authored id (persistence and cross-chunk references)
    // ---------------------------------------------------------------------------------------

    //! Identifies an authored object across sessions and residency cycles.
    //! local = authoring-assigned 32-bit id unique within the chunk; never reused after deletion.
    //! Rule: saves and cross-chunk references use StableId, never AZ::EntityId or pointers.
    struct StableId
    {
        ChunkId chunk;
        uint32_t local = 0;   //!< 0 == invalid
    };

    bool IsValid(const StableId& id) noexcept;
    bool operator==(const StableId& a, const StableId& b) noexcept;
    bool operator!=(const StableId& a, const StableId& b) noexcept;

    // ---------------------------------------------------------------------------------------
    // Pins and degradation
    // ---------------------------------------------------------------------------------------

    //! Why a region is pinned resident. Pinned layers never cancel or deactivate (04 §3.3 rule 3).
    enum class PinReason : uint8_t
    {
        Mission = 0,
        Airlock,
        ScriptedPath,     //!< precomputed preload corridor (prologue bus, race courses)
        SynchronousLoad,
        Debug
    };

    //! Opaque pin identifier. 0 == invalid.
    struct PinId { uint32_t value = 0; };

    //! Streaming deficit response ladder (04 §3.4).
    enum class DegradationLevel : uint8_t
    {
        L0_Normal = 0,
        L1_ShedLowPriority,
        L2_CollisionProxies,
        L3_SpeedSoftCap,
        L4_Stall          //!< must never occur in shipping play; always logged as a defect
    };
} // namespace LowTide::Streaming
