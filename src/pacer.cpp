#include "pacer.hpp"
#include <thread>
#include <algorithm>
#include <immintrin.h>  // _mm_pause

namespace nb {

Pacer::Pacer(uint64_t bitrate_bps, uint32_t packet_size) {
    // interval = (packet_size * 8) / bitrate (seconds), then to ns
    interval_ns_ = (static_cast<double>(packet_size) * 8.0 /
                    static_cast<double>(bitrate_bps)) * 1e9;

    // If interval < 100 us, use burst mode
    constexpr double burst_threshold_ns = 100'000.0; // 100 us
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

    if (delta_ns > 100'000) {
        // > 100 us: use sleep (optimistic wake, then spin for remainder)
        auto sleep_dur = std::chrono::nanoseconds(delta_ns - 90'000);
        std::this_thread::sleep_for(sleep_dur);
    }

    // Spin-wait for the final microseconds
    while (clock::now() < target) {
        _mm_pause();
    }
}

} // namespace nb
