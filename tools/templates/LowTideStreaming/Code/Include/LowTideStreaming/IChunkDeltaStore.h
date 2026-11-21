/*
 * LowTideStreaming - IChunkDeltaStore.h
 *
 * Chunk-local persistent state (AD-09a): delta records against immutable authored content,
 * kept in RAM across unload and written to saves (04 §3.10).
 *
 * DEPENDENCIES: C++17 standard library only.
 *
 * THREADING CONTRACT: main thread only (records originate from gameplay events and from
 * Deactivating flushes, both on the main thread). Not internally synchronized.
 *
 * INVARIANTS:
 *   D1 At most one record per StableId (Record overwrites).
 *   D2 Per chunk count <= config.deltaRecordsPerChunkCap. On overflow, evict the oldest record of
 *      a cosmetic kind (Breakable); gameplay kinds (Container, Pickup, Door, MovedVehicle) are
 *      never evicted. If no cosmetic record exists, Record returns false and logs (data budget bug).
 *   D3 GetDeltas returns a contiguous view valid until the next mutating call on that chunk.
 *   D4 No heap allocation per Record after warm-up (fixed per-chunk capacity).
 */
#pragma once

#include <LowTideStreaming/ChunkTypes.h>

#include <cstddef>
#include <cstdint>

namespace LowTide::Streaming
{
    enum class DeltaKind : uint8_t
    {
        Door = 0,          //!< value: open/closed/locked
        Breakable,         //!< cosmetic; value: broken stage
        Container,         //!< value: looted flag
        Pickup,            //!< value: collected flag
        MovedVehicle,      //!< transform valid
        Count
    };

    struct ChunkDeltaRecord
    {
        uint32_t local = 0;          //!< StableId::local within the chunk
        DeltaKind kind = DeltaKind::Door;
        uint8_t hasTransform = 0;    //!< 1 if position/yaw valid
        uint16_t value = 0;
        Vec3f positionChunkLocal;    //!< meters relative to chunk origin
        float yawRadians = 0.0f;
        uint32_t sequence = 0;       //!< monotonic write order (eviction age)
    };

    class IChunkDeltaStore
    {
    public:
        virtual ~IChunkDeltaStore() = default;

        //! See D1, D2. Returns false only when the cap cannot be satisfied.
        virtual bool Record(const StableId& id, const ChunkDeltaRecord& record) = 0;

        //! See D3. Returns record count; outRecords set to nullptr when zero.
        virtual size_t GetDeltas(ChunkId chunk, const ChunkDeltaRecord*& outRecords) const = 0;

        virtual void ClearChunk(ChunkId chunk) = 0;
        virtual void ClearAll() = 0;

        //! Content hash of the chunk data the deltas were recorded against (04 §3.10 migration).
        virtual void SetContentHash(ChunkId chunk, uint32_t hash) = 0;
        virtual uint32_t GetContentHash(ChunkId chunk) const = 0;
    };
} // namespace LowTide::Streaming
