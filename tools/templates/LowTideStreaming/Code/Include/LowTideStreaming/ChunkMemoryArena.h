/*
 * LowTideStreaming - ChunkMemoryArena.h
 *
 * ChunkBlockPool: one up-front reservation split into uniform blocks (zero external fragmentation).
 * ChunkArena:     per (chunk, layer) linear allocator over pool blocks, freed wholesale.
 *
 * WHAT GOES IN AN ARENA: deserialized ChunkData only (SoA instance arrays, physics primitive
 * descriptors, variant tables, stable-id tables). NEVER engine-owned objects (entities, render
 * handles, physics actors, assets): those are owned via tickets/handles and counted by
 * ChunkBudgetTracker.
 *
 * DEPENDENCIES: C++17 standard library only. The pool's backing reservation is supplied by the
 * caller (engine glue uses the OS/AZ allocator; standalone tests use operator new).
 *
 * THREADING CONTRACT:
 *   ChunkBlockPool: AcquireBlock/ReleaseBlock are thread-safe (internally synchronized) and
 *                   wait-free in the common case. Construction/destruction: main thread only.
 *   ChunkArena:     NOT thread-safe. Exactly one owning thread at a time. Ownership moves
 *                   main -> worker at job dispatch and worker -> main at completion (the job
 *                   system provides happens-before). Never shared concurrently.
 *
 * INVARIANTS:
 *   A1 Allocate never throws; returns nullptr on exhaustion (maxBlocks reached or pool empty).
 *   A2 No single allocation may exceed blockSize - header; larger requests return nullptr and
 *      are a data-validation error (chunk builder must split arrays).
 *   A3 After Release()/destruction, every acquired block is back in the pool.
 *   A4 In debug/profile, blocks carry head/tail guard words verified on release; on mismatch
 *      the arena reports corruption with its AllocationTag.
 *   A5 Byte accounting per ArenaCategory sums to GetBytesUsed().
 *
 * DESIGN REFERENCE: docs/04_World_Streaming_Design.md §3.5.
 */
#pragma once

#include <LowTideStreaming/ChunkTypes.h>

#include <cstddef>
#include <cstdint>

namespace LowTide::Streaming
{
    //! Accounting categories inside a chunk arena (budget report and HUD use these).
    enum class ArenaCategory : uint8_t
    {
        InstanceData = 0,    //!< bulk render instances (mesh id, material id, transforms)
        PhysicsData,         //!< static collision primitive/mesh descriptors
        VariantData,         //!< AD-09 variant group tables
        StableIdTable,       //!< stable id -> local index maps
        Misc,
        Count
    };

    constexpr uint8_t ArenaCategoryCount = static_cast<uint8_t>(ArenaCategory::Count);

    //! Identifies the owner of an arena for leak/corruption reports.
    struct AllocationTag
    {
        ChunkId chunk;
        ChunkLayer layer = ChunkLayer::Gameplay;
        uint16_t generation = 0;
    };

    class ChunkBlockPool
    {
    public:
        //! backingMemory must be blockSizeBytes * blockCount bytes, aligned to 64; caller owns it
        //! and must outlive the pool. blockSizeBytes must be a power of two >= 64 KB.
        ChunkBlockPool(void* backingMemory, size_t blockSizeBytes, uint32_t blockCount) noexcept;
        ~ChunkBlockPool();  //!< asserts all blocks returned (leak = fatal in debug)

        ChunkBlockPool(const ChunkBlockPool&) = delete;
        ChunkBlockPool& operator=(const ChunkBlockPool&) = delete;
        ChunkBlockPool(ChunkBlockPool&&) = delete;
        ChunkBlockPool& operator=(ChunkBlockPool&&) = delete;

        //! Returns a block or nullptr if the pool is empty. Thread-safe.
        void* AcquireBlock() noexcept;
        //! block must come from AcquireBlock of this pool. Thread-safe.
        void ReleaseBlock(void* block) noexcept;

        size_t GetBlockSizeBytes() const noexcept;
        uint32_t GetBlockCount() const noexcept;
        uint32_t GetFreeBlockCount() const noexcept;   //!< relaxed snapshot; for stats only
        uint32_t GetPeakBlocksInUse() const noexcept;
    };

    class ChunkArena
    {
    public:
        ChunkArena(ChunkBlockPool& pool, const AllocationTag& tag, uint32_t maxBlocks) noexcept;
        ~ChunkArena();  //!< calls Release()

        ChunkArena(const ChunkArena&) = delete;
        ChunkArena& operator=(const ChunkArena&) = delete;
        ChunkArena(ChunkArena&&) = delete;            //!< non-movable: tag and block list stay stable
        ChunkArena& operator=(ChunkArena&&) = delete;

        //! Linear allocation. alignment must be a power of two <= 64. See invariants A1, A2.
        void* Allocate(size_t bytes, size_t alignment, ArenaCategory category) noexcept;

        //! Returns all blocks to the pool; arena becomes empty and reusable with the same tag.
        void Release() noexcept;

        //! Debug/profile: verifies guard words of every held block. Returns false on corruption.
        bool VerifyIntegrity() const noexcept;

        size_t GetBytesUsed() const noexcept;
        size_t GetBytesUsed(ArenaCategory category) const noexcept;
        uint32_t GetBlocksHeld() const noexcept;
        const AllocationTag& GetTag() const noexcept;
    };
} // namespace LowTide::Streaming
