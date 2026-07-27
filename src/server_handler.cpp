#include "server_handler.hpp"
#include "udp_socket.hpp"
#include "protocol_header.hpp"
#include "stats_collector.hpp"
#include "control_protocol.hpp"
#include "reporter.hpp"
#include "console_handler.hpp"
#include <nlohmann/json.hpp>

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <cstring>

namespace nb {

void run_server(const Config& cfg) {
    auto reporter = std::make_unique<Reporter>(cfg);

    // ── Create and bind socket ──────────────────────────────
    auto sock = std::make_unique<UdpSocket>(cfg.ipv6, cfg.bind_addr, cfg.port);
    sock->set_recv_timeout(std::chrono::seconds(cfg.idle_timeout_sec));

    reporter->report_start(cfg);

    // ── State machine loop ──────────────────────────────────
    // States: LISTENING → RECEIVING → REPORTING → IDLE → (+ loop or exit)

    constexpr size_t BUF_SIZE = MAX_UDP_PAYLOAD;
    auto buf = std::make_unique<uint8_t[]>(BUF_SIZE);
    auto server_stats = std::make_unique<StatsCollector>();
    auto client_addr = sockaddr_storage{};
    bool client_addr_valid = false;
    uint64_t test_start_ns = 0;
    uint32_t test_duration_sec = 0;

    while (!g_shutdown.load(std::memory_order_relaxed)) {
        // ── LISTENING: wait for Start message ───────────────
        reporter->report_info("Listening for client...");

        while (!g_shutdown.load(std::memory_order_relaxed)) {
            sockaddr_storage from{};
            int n = sock->recv_from(buf.get(), BUF_SIZE, &from);
            if (n < 0) continue; // timeout, check shutdown
            if (static_cast<size_t>(n) < HEADER_SIZE) continue;

            auto hdr_opt = header_from_wire(buf.get(), static_cast<size_t>(n));
            if (!hdr_opt) continue;

            auto& hdr = *hdr_opt;
            if (hdr.msg_type != MSG_START) continue;

            // Found a Start message — extract duration
            test_duration_sec = 0;
            if (static_cast<size_t>(n) >= HEADER_SIZE + 4) {
                test_duration_sec = ControlProtocol::parse_start_duration(
                    buf.get() + HEADER_SIZE,
                    static_cast<size_t>(n) - HEADER_SIZE);
            }
            if (test_duration_sec == 0) test_duration_sec = 10;

            client_addr = from;
            client_addr_valid = true;
            auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            test_start_ns = now_ns;

            server_stats->start_test(test_duration_sec);
            reporter->report_info("Client connected, starting " +
                                   std::to_string(test_duration_sec) + "s test");

            break; // enter RECEIVING
        }

        if (g_shutdown) break;

        // ── RECEIVING: collect data packets ─────────────────
        {
            auto deadline = std::chrono::steady_clock::now() +
                std::chrono::seconds(test_duration_sec + 5);

            while (!g_shutdown.load(std::memory_order_relaxed)) {
                if (std::chrono::steady_clock::now() >= deadline) {
                    reporter->report_info("Test timeout — no Finish received");
                    break;
                }

                sockaddr_storage from{};
                int n = sock->recv_from(buf.get(), BUF_SIZE, &from);
                if (n < 0) {
                    // timeout — check deadline
                    continue;
                }
                if (static_cast<size_t>(n) < HEADER_SIZE) continue;
                if (client_addr_valid &&
                    std::memcmp(&from, &client_addr, sizeof(from)) != 0) {
                    continue; // ignore packets from other sources
                }

                auto hdr_opt = header_from_wire(buf.get(), static_cast<size_t>(n));
                if (!hdr_opt) continue;
                auto& hdr = *hdr_opt;

                if (hdr.msg_type == MSG_FINISH) {
                    reporter->report_info("Finish received");

                    // Extract total_packets from Finish payload
                    size_t payload_len = static_cast<size_t>(n) - HEADER_SIZE;
                    uint64_t sender_total = ControlProtocol::parse_finish_total_packets(
                        buf.get() + HEADER_SIZE, payload_len);
                    if (sender_total > 0) {
                        server_stats->set_sender_packets(sender_total);
                        reporter->report_info("Sender reported " +
                            std::to_string(sender_total) + " packets total");
                    }

                    break; // → REPORTING
                }

                if (hdr.msg_type == MSG_DATA) {
                    auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                    server_stats->record_packet(hdr, now_ns);
                }
            }
        }

        if (g_shutdown) break;

        // ── REPORTING: compute stats and send Result ────────
        {
            StatsSummary summary = server_stats->finalize();

            // Set sender packet count from what we tracked
            summary.packets_sent = summary.total_packets;
            summary.bytes_received = summary.bytes_received;

            int ack_count = ControlProtocol::send_result(*sock, client_addr, summary);

            reporter->report_summary(summary, nullptr); // server-side display

            if (ack_count > 0) {
                reporter->report_info("Result ACK'd after " +
                                       std::to_string(ack_count) + " attempt(s)");
            } else {
                reporter->report_info("Result sent (no ACK)");
            }
        }

        // ── IDLE/EXIT ───────────────────────────────────────
        if (cfg.single_shot) {
            break; // exit after one test
        }

        // Reset for next client
        client_addr_valid = false;
        reporter->report_info("Ready for next client...");
    }

    reporter->report_info("Server shutting down.");
}

} // namespace nb
