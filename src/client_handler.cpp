#include "client_handler.hpp"
#include "udp_socket.hpp"
#include "protocol_header.hpp"
#include "pacer.hpp"
#include "stats_collector.hpp"
#include "control_protocol.hpp"
#include "reporter.hpp"
#include "console_handler.hpp"

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>

namespace nb {

// ── Resolve hostname ────────────────────────────────────────
static sockaddr_storage resolve_host(const std::string& host, uint16_t port,
                                     bool ipv6) {
    sockaddr_storage addr{};
    ADDRINFO hints{};
    ADDRINFO* result = nullptr;

    hints.ai_family = ipv6 ? AF_INET6 : AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    std::string port_str = std::to_string(port);

    int rc = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result);
    if (rc != 0 || result == nullptr) {
        if (result) freeaddrinfo(result);
        throw std::runtime_error("Failed to resolve " + host + ": " +
                                 gai_strerror(rc));
    }

    std::memcpy(&addr, result->ai_addr, result->ai_addrlen);
    freeaddrinfo(result);
    return addr;
}

void run_client(const Config& cfg) {
    auto reporter = std::make_unique<Reporter>(cfg);

    // ── Resolve target ──────────────────────────────────────
    sockaddr_storage server_addr = resolve_host(
        cfg.server_host, cfg.port, cfg.ipv6);

    // ── Single socket for both send and receive ─────────────
    // This ensures the server's Result message arrives at the same port
    // we're listening on.
    auto sock = UdpSocket::create_client(cfg.ipv6);
    sock->connect(server_addr);

    // ── Initialize ──────────────────────────────────────────
    auto pacer = std::make_unique<Pacer>(cfg.bitrate_bps, cfg.packet_len);

    std::atomic<bool> test_completed{false};
    std::atomic<bool> result_received{false};
    StatsSummary server_summary{};
    uint64_t packets_sent = 0;
    uint64_t local_bytes = 0;

    reporter->report_start(cfg);

    // ── Allocate reusable send buffer ─────────────────────────
    // Pre-filled with 'D' payload bytes; only the 24-byte header
    // is overwritten per-packet, avoiding a heap allocation on every send.
    std::vector<uint8_t> send_buf(cfg.packet_len, 'D');
    auto now_ns = []() -> uint64_t {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    };

    // ── Send Start ──────────────────────────────────────────
    ControlProtocol::send_start(*sock, server_addr, cfg.duration_sec);

    auto test_start_time = Pacer::clock::now();
    auto test_end_time = test_start_time +
        std::chrono::seconds(cfg.duration_sec);

    // ── Sender thread ───────────────────────────────────────
    std::thread sender([&]() {
        uint32_t packet_id = 1;
        auto next_send = test_start_time;
        auto prev_interval_time = test_start_time;
        uint64_t interval_bytes = 0;
        int interval_num = 0;

        while (!g_shutdown.load(std::memory_order_relaxed) &&
               !test_completed.load(std::memory_order_relaxed)) {
            auto now = Pacer::clock::now();

            // Check for per-interval reporting FIRST, so the final
            // interval (e.g. 4-5s for -t 5) fires before the test ends.
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - prev_interval_time).count();
            if (elapsed >= cfg.interval_sec) {
                IntervalSnapshot snap;
                snap.stream_id = 1;
                snap.start_sec = static_cast<double>(interval_num * cfg.interval_sec);
                snap.end_sec   = static_cast<double>((interval_num + 1) * cfg.interval_sec);
                snap.bytes     = interval_bytes;
                snap.bits_per_second = cfg.interval_sec > 0
                    ? static_cast<uint64_t>(static_cast<double>(interval_bytes * 8) / cfg.interval_sec)
                    : 0;
                snap.jitter_ms     = 0;
                snap.lost_packets  = 0;
                snap.total_packets = 0;
                snap.lost_percent  = 0;
                reporter->report_interval(snap);

                interval_bytes = 0;
                interval_num++;
                prev_interval_time = now;
            }

            // Check if test time elapsed (after interval reporting so the
            // final interval is always shown before the test ends)
            if (now >= test_end_time) {
                test_completed.store(true, std::memory_order_release);
                break;
            }

            // Build protocol header and write into reusable buffer
            auto hdr = make_data_header(packet_id,
                                        cfg.packet_len - HEADER_SIZE,
                                        now_ns());
            header_to_wire(hdr, send_buf.data());

            // Send with pacing
            pacer->wait_until(next_send);
            try {
                sock->send_to(send_buf.data(), send_buf.size(), server_addr);
            } catch (const std::system_error& e) {
                // send_to failure is fatal for UDP (socket is blocking, so
                // transient buffer-full cannot occur — any failure indicates
                // a real problem such as a closed/broken socket).
                reporter->report_error("Send error: " + std::string(e.what()));
                test_completed.store(true, std::memory_order_release);
                break;
            }
            packets_sent++;
            local_bytes += cfg.packet_len;
            interval_bytes += cfg.packet_len;

            // Check for ICMP error
            int err = sock->check_error();
            if (err != 0) {
                reporter->report_error("ICMP error: server may be unreachable");
                test_completed.store(true, std::memory_order_release);
                break;
            }

            // Advance next send time — only once per burst
            // (burst_size=1 means every packet, larger means back-to-back then wait)
            packet_id++;
            if (packet_id == 0) packet_id = 1;  // 0 is reserved for control messages
            if (packet_id % pacer->burst_size() == 0) {
                next_send += std::chrono::nanoseconds(
                    static_cast<int64_t>(pacer->interval_ns() * pacer->burst_size()));
            }
        }

        // ── Report trailing partial interval ─────────────────
        // The main while loop checks elapsed >= interval_sec using
        // duration_cast<seconds> (integer truncation). If the test ends
        // mid-interval (e.g. elapsed = trunc(0.95s) = 0), the check fails
        // and the last ~0.95s of data is never interval-reported.
        // Report it here with the actual wall-clock duration so the bitrate
        // is correct and interval_bytes sum matches local_bytes exactly.
        if (interval_bytes > 0) {
            auto now = Pacer::clock::now();
            double actual_dur = std::chrono::duration_cast<
                std::chrono::duration<double>>(now - prev_interval_time).count();

            IntervalSnapshot snap;
            snap.stream_id = 1;
            snap.start_sec = static_cast<double>(interval_num * cfg.interval_sec);
            // Use the configured duration as end label so the trailing
            // interval shows e.g. "4.00-5.00 sec" matching user expectations.
            // The bitrate still uses actual_dur so it remains accurate.
            snap.end_sec   = static_cast<double>(cfg.duration_sec);
            snap.bytes     = interval_bytes;
            snap.bits_per_second = actual_dur > 0
                ? static_cast<uint64_t>(static_cast<double>(interval_bytes * 8) / actual_dur)
                : 0;
            snap.jitter_ms     = 0;
            snap.lost_packets  = 0;
            snap.total_packets = 0;
            snap.lost_percent  = 0;
            reporter->report_interval(snap);
        }

        // ── Send Finish (with retry, early exit on Result) ──
        for (int i = 0; i < 5; ++i) {
            if (g_shutdown) break;
            if (result_received.load(std::memory_order_acquire)) break;  // server responded
            ControlProtocol::send_finish(*sock, server_addr, packets_sent);

            // Brief wait between retries (also gives Result a chance to arrive)
            for (int w = 0; w < 10 && !result_received.load(std::memory_order_acquire); ++w) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        test_completed.store(true, std::memory_order_release);
    });

    // ── Receiver thread ─────────────────────────────────────
    std::thread receiver([&]() {
        try {
            constexpr size_t BUF_SIZE = 2048;
            auto buf = std::make_unique<uint8_t[]>(BUF_SIZE);

            auto deadline = std::chrono::steady_clock::now() +
                std::chrono::seconds(cfg.duration_sec + 5);

            while (!g_shutdown.load(std::memory_order_relaxed) &&
                   !result_received.load(std::memory_order_acquire) &&
                   !test_completed.load(std::memory_order_relaxed) &&
                   std::chrono::steady_clock::now() < deadline) {

                // Wait for data with timeout
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(sock->native_handle(), &readfds);

                struct timeval tv;
                tv.tv_sec = 0;
                tv.tv_usec = 100000;  // 100ms timeout (check shutdown flag)

                int rc = select(0, &readfds, nullptr, nullptr, &tv);
                if (rc <= 0) continue;

                sockaddr_storage from{};
                int n = sock->recv_from(buf.get(), BUF_SIZE, &from);
                if (n < 0) continue;
                if (static_cast<size_t>(n) < HEADER_SIZE) continue;

                auto hdr_opt = header_from_wire(buf.get(), static_cast<size_t>(n));
                if (!hdr_opt) continue;

                if (hdr_opt->msg_type == MSG_RESULT) {
                    size_t payload_len = n - HEADER_SIZE;
                    server_summary = ControlProtocol::parse_result(
                        buf.get() + HEADER_SIZE, payload_len);
                    result_received.store(true, std::memory_order_release);
                    reporter->report_info("Server result received");
                }
            }
        } catch (const std::exception& e) {
            reporter->report_error("Receiver error: " + std::string(e.what()));
        }
    });

    // ── Wait for sender to finish ───────────────────────────
    sender.join();

    // ── Wait for receiver to finish ─────────────────────────
    // Receiver will exit within 100ms (select timeout) once
    // test_completed, result_received, or g_shutdown is set.
    if (receiver.joinable()) {
        receiver.join();
    }

    // ── Report summary ──────────────────────────────────────
    double elapsed = cfg.duration_sec;

    StatsSummary local_summary{};
    local_summary.duration_sec     = elapsed;
    local_summary.bytes_received   = local_bytes;
    local_summary.packets_sent     = packets_sent;
    // packets_received stays 0 — sender doesn't receive data packets
    local_summary.total_packets    = packets_sent;
    local_summary.bits_per_second  = elapsed > 0
        ? static_cast<uint64_t>(local_bytes * 8 / elapsed) : 0;

    if (result_received.load(std::memory_order_acquire)) {
        reporter->report_summary(local_summary, &server_summary);
    } else {
        reporter->report_summary(local_summary, nullptr);
        reporter->report_info("No server result received — local stats only");
    }
}

} // namespace nb
