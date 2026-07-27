#include "pacer.hpp"
#include <thread>
#include <algorithm>
#include <immintrin.h>  // _mm_pause

namespace nb {

Pacer::Pacer(uint64_t bitrate_bps, uint32_t packet_size) {
    // interval = (packet_size * 8) / bitrate (seconds), then to ns
    interval_ns_ = (static_cast<double>(packet_size) * 8.0 /
                    static_cast<double>(bitrate_bps)) * 1e9;

    // If interval < 2 ms, use burst mode so the per-burst wait exceeds
    // kMinSleepNs and can use sleep_for rather than spinning the full gap.
    // This matches the sleep/spin threshold in wait_until().
    constexpr double burst_threshold_ns = 2'000'000.0; // 2 ms
    if (interval_ns_ > 0 && interval_ns_ < burst_threshold_ns) {
        burst_size_ = static_cast<uint32_t>(
            std::ceil(burst_threshold_ns / interval_ns_));
    } else {
        burst_size_ = 1;
    }
}

void Pacer::wait_until(clock::time_point target) {
    auto now = clock::now();
    if (now >= target) return;  // behind schedule — skip wait

    auto delta_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        target - now).count();

    // On Windows, sleep_for(< 2 ms) is unreliable due to coarse OS timer
    // granularity (~1-16 ms default).
    //
    // Strategy (priority: accuracy for high bitrates):
    //   <= 2 ms   - spin-wait only (no OS sleep) → accurate nanosecond pacing
    //                used for burst-mode waits (~106 µs at 1 Gbps)
    //   > 2 ms    - sleep for bulk, spin for remainder
    //                moderate overshoot tolerated (low-rate / non-burst)
    //                achieved throughput is always reported correctly
    constexpr int64_t kMinSleepNs = 2'000'000;  // 2 ms

    if (delta_ns > kMinSleepNs) {
        // Sleep for the bulk, leaving ~100 µs for spin compensation.
        // Note: on default Windows timer ~15.6 ms, a sleep_for(< 16 ms)
        // may round up to the next tick. The achieved throughput is still
        // measured and reported correctly — this only affects -b accuracy.
        auto sleep_dur = std::chrono::nanoseconds(delta_ns - 100'000);
        std::this_thread::sleep_for(sleep_dur);
    }

    // Spin-wait for the remaining time.
    // For delta > 2 ms (low bitrate / non-burst): spin ~100 µs after sleep
    // to absorb wake-up imprecision — negligible CPU.
    // For delta <= 2 ms (burst mode, high bitrate): the full wait is
    // spin-waited for accurate nanosecond pacing. The spin duration is
    // at most ~2 ms per burst (advance - actual send time). This is
    // acceptable — the tool is a network benchmark and is expected
    // to consume CPU during active measurement.
    // If sleep overshot target: loop exits immediately (no busy-wait).
    while (clock::now() < target) {
        _mm_pause();
    }
}

} // namespace nb
