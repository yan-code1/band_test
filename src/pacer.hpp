#pragma once

#include <chrono>
#include <cstdint>

namespace nb {

/// Hybrid sleep/spin-wait bandwidth pacer.
class Pacer {
public:
    using clock = std::chrono::high_resolution_clock;

    /// Construct with target bitrate and packet size.
    Pacer(uint64_t bitrate_bps, uint32_t packet_size);

    /// Wait until the next send time.
    void wait_until(clock::time_point target);

    /// Get the per-packet interval in nanoseconds.
    double interval_ns() const { return interval_ns_; }

    /// Get burst size (number of packets to send back-to-back).
    uint32_t burst_size() const { return burst_size_; }

private:
    double   interval_ns_;    // nanoseconds between packets
    uint32_t burst_size_ = 1; // >= 1
};

} // namespace nb
