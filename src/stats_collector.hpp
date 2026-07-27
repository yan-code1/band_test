#pragma once

#include "protocol_header.hpp"

#include <cstdint>
#include <mutex>
#include <vector>
#include <set>

namespace nb {

/// Per-interval statistics snapshot.
struct IntervalSnapshot {
    uint32_t stream_id = 1;
    double   start_sec = 0;
    double   end_sec   = 0;
    uint64_t bytes         = 0;
    uint64_t bits_per_second = 0;
    double   jitter_ms     = 0;
    double   jitter_min_ms = 0;
    double   jitter_max_ms = 0;
    uint64_t lost_packets   = 0;
    uint64_t total_packets  = 0;
    double   lost_percent   = 0;
    uint32_t out_of_order     = 0;
    uint32_t duplicate_packets = 0;
};

/// Final test summary.
struct StatsSummary {
    double   duration_sec = 0;
    uint64_t bytes_received = 0;
    uint64_t packets_received = 0;
    uint64_t packets_sent = 0;
    uint64_t bits_per_second = 0;
    double   jitter_ms = 0;
    double   jitter_min_ms = 0;
    double   jitter_max_ms = 0;
    uint64_t lost_packets = 0;
    uint64_t total_packets = 0;
    double   lost_percent = 0;
    uint32_t out_of_order = 0;
    uint32_t duplicate_packets = 0;
};

/// Thread-safe statistics collector.
class StatsCollector {
public:
    StatsCollector() = default;

    /// Record a received data packet.
    void record_packet(const ProtocolHeader& hdr,
                       uint64_t recv_time_ns);

    /// Called when the test is complete — finalize stats.
    StatsSummary finalize();

    /// Get a snapshot for the current interval (resets counters).
    IntervalSnapshot next_interval(double elapsed_sec, double interval_dur);

    /// Mark the start of a test.
    void start_test(uint32_t duration_sec);

    /// Set the total number of packets the sender transmitted.
    void set_sender_packets(uint64_t sent);

private:
    std::mutex mtx_;

    // State
    uint64_t first_packet_time_ns_ = 0;
    uint64_t last_packet_time_ns_ = 0;
    uint64_t bytes_received_ = 0;
    uint64_t packets_received_ = 0;
    uint32_t last_packet_id_ = 0;
    uint32_t out_of_order_ = 0;
    uint32_t duplicate_ = 0;
    uint64_t total_packets_ = 0;  // from sender

    // Test configuration
    uint64_t test_duration_ns_ = 0;  // from start_test()

    // Jitter (RFC 3550)
    double jitter_ = 0;
    double jitter_min_ = 1e9;
    double jitter_max_ = 0;
    int64_t prev_transit_ = 0;
    bool    first_packet_ = true;

    // Per-interval counters (reset each interval)
    uint64_t interval_bytes_ = 0;
    uint64_t interval_packets_ = 0;

    std::set<uint32_t> seen_ids_;  // duplicate detection
};

} // namespace nb
