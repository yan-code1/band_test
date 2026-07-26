#pragma once

#include "protocol_header.hpp"
#include "stats_collector.hpp"
#include "udp_socket.hpp"

#include <cstdint>
#include <vector>

namespace nb {

/// Construct and parse control messages (Start/Finish/Result).
class ControlProtocol {
public:
    /// Create a Start message payload (8 bytes: duration_sec + reserved).
    static std::vector<uint8_t> make_start_payload(uint32_t duration_sec);

    /// Parse duration from Start payload (expects 8 bytes).
    static uint32_t parse_start_duration(const uint8_t* data, size_t len);

    /// Create a Result payload (JSON summary, max 1024 bytes).
    static std::vector<uint8_t> make_result_payload(const StatsSummary& summary);

    /// Parse Result payload into StatsSummary.
    static StatsSummary parse_result(const uint8_t* data, size_t len);

    /// Send Start message to target.
    static void send_start(UdpSocket& sock, const sockaddr_storage& dest,
                           uint32_t duration_sec);

    /// Send Finish message to target.
    static void send_finish(UdpSocket& sock, const sockaddr_storage& dest);

    /// Send Result message (with retry).
    /// Returns true if ACK received.
    static int send_result(UdpSocket& sock, const sockaddr_storage& dest,
                           const StatsSummary& summary);

    /// Send an empty ACK packet (one zero byte).
    static void send_ack(UdpSocket& sock, const sockaddr_storage& dest);
};

} // namespace nb
