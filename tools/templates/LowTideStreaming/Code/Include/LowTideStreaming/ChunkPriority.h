/*
 * LowTideStreaming - ChunkPriority.h
 *
 * Pure priority and degradation functions (04 §3.3, §3.4). No state, no engine dependency, so
 * they are unit-testable in isolation (docs/05 §4.3 U5, U6, U10, U11).
 *
 * DEPENDENCIES: C++17 standard library only.
 *
 * THREADING CONTRACT: pure functions; reentrant; callable from any thread.
 */
#pragma once

#include <LowTideStreaming/ChunkStreamingConfig.h>
#include <LowTideStreaming/ChunkTypes.h>

#include <cstdint>

namespace LowTide::Streaming
{
    //! Inputs for one candidate (chunk, layer). Built by the scheduler from SoA data each frame.
    struct PriorityInputs
    {
        Vec2f focusPosition;                 //!< player (or camera for S/D/H1 if farther ahead)
        Vec2f focusVelocity;                 //!< m/s, ground plane
        AabbXY chunkBounds;
        ChunkLayer layer = ChunkLayer::Gameplay;
        bool pinned = false;
        float roadPathDistanceMeters = -1.0f; //!< distance along predicted road path; < 0 = not on path
    };

    //! Sort key: ascending order = load first. pinClass 0 (pinned) always precedes 1.
    struct PriorityKey
    {
        uint8_t pinClass = 1;
        float weightedEtaSeconds = 0.0f;
    };

    bool operator<(const PriorityKey& a, const PriorityKey& b) noexcept;

    //! eta = min(d / max(v_along, walkFloor), roadDist / max(speed, walkFloor)); key = (pin, eta * layerFactor).
    PriorityKey ComputePriority(const PriorityInputs& inputs, const ChunkStreamingConfig& config) noexcept;

    //! R_in_eff = R_in + |v| * lookAheadSeconds * max(0, cos(theta)) toward the chunk. Never < R_in.
    float ComputeEffectiveRadiusIn(const PriorityInputs& inputs, const ChunkStreamingConfig& config) noexcept;

    //! Degradation level from the minimum G-layer slack and proxy availability (04 §3.4 table).
    //! Applies hysteresis: stepping down a level requires slack > threshold * 1.5 for >= 1 s
    //! (caller passes secondsAboveThreshold).
    DegradationLevel SelectDegradationLevel(
        float minGameplaySlackSeconds,
        float nearestUncoveredGameplayMeters,   //!< nearest G chunk with neither Active nor proxy
        DegradationLevel current,
        float secondsAboveThreshold,
        const ChunkStreamingConfig& config) noexcept;

    //! v_cap = max(softCapMinSpeedMps, (dUnready - softCapMarginMeters) / tReadyEstSeconds).
    //! Returns 0 when level < L3 (no cap). Monotonic non-decreasing in dUnreadyMeters.
    float ComputeSpeedCapMps(
        DegradationLevel level,
        float dUnreadyMeters,
        float tReadyEstSeconds,
        const ChunkStreamingConfig& config) noexcept;
} // namespace LowTide::Streaming
