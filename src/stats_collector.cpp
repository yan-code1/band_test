#include "stats_collector.hpp"
#include <cmath>
#include <algorithm>

namespace nb {

static constexpr double NANOS_PER_SEC = 1'000'000'000.0;

void StatsCollector::start_test(uint32_t /*duration_sec*/) {
    std::lock_guard<std::mutex> lock(mtx_);
    first_packet_time_ns_ = 0;
    last_packet_time_ns_ = 0;
    bytes_received_ = 0;
    packets_received_ = 0;
    last_packet_id_ = 0;
    out_of_order_ = 0;
    duplicate_ = 0;
    jitter_ = 0;
    jitter_min_ = 1e9;
    jitter_max_ = 0;
    prev_transit_ = 0;
    first_packet_ = true;
    interval_bytes_ = 0;
    interval_packets_ = 0;
    seen_ids_.clear();
    total_packets_ = 0;
}

void StatsCollector::set_sender_packets(uint64_t sent) {
    std::lock_guard<std::mutex> lock(mtx_);
    total_packets_ = sent;
}

void StatsCollector::record_packet(const ProtocolHeader& hdr,
                                   uint64_t recv_time_ns) {
    std::lock_guard<std::mutex> lock(mtx_);

    uint32_t pid = hdr.packet_id;

    // First packet initializes tracking
    if (first_packet_) {
        first_packet_time_ns_ = recv_time_ns;
        last_packet_id_ = pid;
        first_packet_ = false;

        // Compute initial transit for jitter
        uint64_t send_time = static_cast<uint64_t>(hdr.timestamp_sec) * 1'000'000'000 +
                             hdr.timestamp_nsec;
        prev_transit_ = static_cast<int64_t>(recv_time_ns - send_time);
    }

    // Duplicate detection
    if (!seen_ids_.insert(pid).second) {
        duplicate_++;
        return;  // don't count duplicates in stats
    }

    // Out-of-order detection
    if (pid < last_packet_id_) {
        out_of_order_++;
    }
    last_packet_id_ = pid;

    // Jitter calculation (RFC 3550)
    uint64_t send_time = static_cast<uint64_t>(hdr.timestamp_sec) * 1'000'000'000 +
                         hdr.timestamp_nsec;
    int64_t transit = static_cast<int64_t>(recv_time_ns - send_time);
    int64_t delta = transit - prev_transit_;
    if (delta < 0) delta = -delta;
    double delta_ms = static_cast<double>(delta) / 1'000'000.0;
    jitter_ += (delta_ms - jitter_) / 16.0;
    jitter_min_ = std::min(jitter_min_, jitter_);
    jitter_max_ = std::max(jitter_max_, jitter_);
    prev_transit_ = transit;

    // Byte/packet counters
    uint32_t payload_bytes = hdr.total_length;
    bytes_received_ += payload_bytes;
    interval_bytes_ += payload_bytes;
    packets_received_++;
    interval_packets_++;

    last_packet_time_ns_ = recv_time_ns;
}

IntervalSnapshot StatsCollector::next_interval(double elapsed_sec,
                                               double interval_dur) {
    std::lock_guard<std::mutex> lock(mtx_);

    IntervalSnapshot snap;
    snap.start_sec = elapsed_sec - interval_dur;
    snap.end_sec   = elapsed_sec;
    snap.bytes     = interval_bytes_;
    snap.bits_per_second = interval_dur > 0
        ? static_cast<uint64_t>(static_cast<double>(interval_bytes_ * 8) / interval_dur)
        : 0;
    snap.jitter_ms     = jitter_;
    snap.jitter_min_ms = jitter_min_ >= 1e8 ? 0 : jitter_min_;
    snap.jitter_max_ms = jitter_max_;

    // Loss detection — infer from max packet_id if total_packets unknown
    if (total_packets_ > 0) {
        snap.total_packets = static_cast<uint32_t>(total_packets_);
        snap.lost_packets = total_packets_ - packets_received_;
    } else if (last_packet_id_ > 0) {
        snap.total_packets = last_packet_id_;
        snap.lost_packets = last_packet_id_ - packets_received_;
    }
    snap.lost_percent = snap.total_packets > 0
        ? (static_cast<double>(snap.lost_packets) / snap.total_packets) * 100.0
        : 0.0;

    snap.out_of_order = out_of_order_;
    snap.duplicate_packets = duplicate_;

    // Reset interval counters
    interval_bytes_ = 0;
    interval_packets_ = 0;

    return snap;
}

StatsSummary StatsCollector::finalize() {
    std::lock_guard<std::mutex> lock(mtx_);

    StatsSummary s;
    s.duration_sec = (last_packet_time_ns_ - first_packet_time_ns_) / NANOS_PER_SEC;
    if (s.duration_sec < 0.001) s.duration_sec = 0.001;
    s.bytes_received  = bytes_received_;
    s.packets_received = packets_received_;
    s.packets_sent     = total_packets_;
    s.bits_per_second  = static_cast<uint64_t>(
        static_cast<double>(bytes_received_ * 8) / s.duration_sec);
    s.jitter_ms     = jitter_;
    s.jitter_min_ms = jitter_min_ >= 1e8 ? 0 : jitter_min_;
    s.jitter_max_ms = jitter_max_;
    if (total_packets_ > 0) {
        s.total_packets = total_packets_;
    } else if (last_packet_id_ > 0) {
        s.total_packets = last_packet_id_;
    }
    s.lost_packets  = s.total_packets > packets_received_
        ? s.total_packets - packets_received_ : 0;
    s.lost_percent  = s.total_packets > 0
        ? (static_cast<double>(s.lost_packets) / s.total_packets) * 100.0
        : 0.0;
    s.out_of_order     = out_of_order_;
    s.duplicate_packets = duplicate_;

    return s;
}

} // namespace nb
