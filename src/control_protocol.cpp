#include "control_protocol.hpp"
#include "platform.hpp"
#include <nlohmann/json.hpp>

#include <cstring>
#include <thread>
#include <chrono>

namespace nb {

std::vector<uint8_t> ControlProtocol::make_start_payload(uint32_t duration_sec) {
    std::vector<uint8_t> payload(8, 0);
    std::memcpy(payload.data(), &duration_sec, sizeof(duration_sec));
    return payload;
}

uint32_t ControlProtocol::parse_start_duration(const uint8_t* data, size_t len) {
    if (len < sizeof(uint32_t)) return 0;
    uint32_t dur = 0;
    std::memcpy(&dur, data, sizeof(dur));
    return dur;
}

std::vector<uint8_t> ControlProtocol::make_result_payload(
    const StatsSummary& s) {
    nlohmann::json j;
    j["duration_sec"]       = s.duration_sec;
    j["bytes_received"]     = s.bytes_received;
    j["packets_received"]   = s.packets_received;
    j["packets_sent"]       = s.packets_sent;
    j["bits_per_second"]    = s.bits_per_second;
    j["jitter_ms"]          = s.jitter_ms;
    j["jitter_min_ms"]      = s.jitter_min_ms;
    j["jitter_max_ms"]      = s.jitter_max_ms;
    j["lost_packets"]       = s.lost_packets;
    j["total_packets"]      = s.total_packets;
    j["lost_percent"]       = s.lost_percent;
    j["out_of_order"]       = s.out_of_order;
    j["duplicate_packets"]  = s.duplicate_packets;

    std::string json_str = j.dump();
    if (json_str.size() > 1024) {
        json_str.resize(1024);  // truncate to fit UDP
    }

    return std::vector<uint8_t>(json_str.begin(), json_str.end());
}

StatsSummary ControlProtocol::parse_result(const uint8_t* data, size_t len) {
    StatsSummary s{};
    try {
        std::string json_str(reinterpret_cast<const char*>(data), len);
        auto j = nlohmann::json::parse(json_str);
        s.duration_sec      = j.value("duration_sec", 0.0);
        s.bytes_received    = j.value("bytes_received", 0ULL);
        s.packets_received  = j.value("packets_received", 0ULL);
        s.packets_sent      = j.value("packets_sent", 0ULL);
        s.bits_per_second   = j.value("bits_per_second", 0ULL);
        s.jitter_ms         = j.value("jitter_ms", 0.0);
        s.jitter_min_ms     = j.value("jitter_min_ms", 0.0);
        s.jitter_max_ms     = j.value("jitter_max_ms", 0.0);
        s.lost_packets      = j.value("lost_packets", 0ULL);
        s.total_packets     = j.value("total_packets", 0ULL);
        s.lost_percent      = j.value("lost_percent", 0.0);
        s.out_of_order      = j.value("out_of_order", 0U);
        s.duplicate_packets = j.value("duplicate_packets", 0U);
    } catch (...) {
        // parse error — return empty summary
    }
    return s;
}

void ControlProtocol::send_start(UdpSocket& sock,
                                 const sockaddr_storage& dest,
                                 uint32_t duration_sec) {
    auto hdr = make_start_header(duration_sec);
    auto payload = make_start_payload(duration_sec);

    std::vector<uint8_t> buf(HEADER_SIZE + payload.size());
    header_to_wire(hdr, buf.data());
    std::memcpy(buf.data() + HEADER_SIZE, payload.data(), payload.size());

    sock.send_to(buf.data(), buf.size(), dest);
}

void ControlProtocol::send_finish(UdpSocket& sock,
                                  const sockaddr_storage& dest,
                                  uint64_t total_packets) {
    auto hdr = make_finish_header();
    std::vector<uint8_t> buf(HEADER_SIZE + 8);
    header_to_wire(hdr, buf.data());
    std::memcpy(buf.data() + HEADER_SIZE, &total_packets, sizeof(total_packets));
    sock.send_to(buf.data(), buf.size(), dest);
}

uint64_t ControlProtocol::parse_finish_total_packets(const uint8_t* data,
                                                      size_t len) {
    if (len < sizeof(uint64_t)) return 0;
    uint64_t total = 0;
    std::memcpy(&total, data, sizeof(total));
    return total;
}

int ControlProtocol::send_result(UdpSocket& sock,
                                 const sockaddr_storage& dest,
                                 const StatsSummary& summary) {
    auto payload = make_result_payload(summary);
    auto hdr = make_result_header(static_cast<uint32_t>(payload.size()));

    std::vector<uint8_t> buf(HEADER_SIZE + payload.size());
    header_to_wire(hdr, buf.data());
    std::memcpy(buf.data() + HEADER_SIZE, payload.data(), payload.size());

    // Send with retry: up to 3 times, wait 200ms for ACK
    // Use a large enough buffer to discard any stray packets gracefully
    for (int attempt = 0; attempt < 3; ++attempt) {
        sock.send_to(buf.data(), buf.size(), dest);

        // Wait for ACK — use large buffer to avoid WSAEMSGSIZE
        uint8_t ack_buf[2048];
        sockaddr_storage from{};
        sock.set_recv_timeout(std::chrono::milliseconds(200));

        // Send an empty ACK packet (client does this when it receives Result)
        // We just check for *any* incoming packet as implicit ACK
        // (client sends a 1-byte zero as explicit ACK)
        {
            int rc = sock.recv_from(ack_buf, sizeof(ack_buf), nullptr);
            if (rc > 0) {
                return attempt + 1;  // ACK received
            }
            // rc == -1 means timeout — retry
        }
    }
    return 0;  // no ACK
}

void ControlProtocol::send_ack(UdpSocket& sock,
                               const sockaddr_storage& dest) {
    uint8_t ack = 0;
    sock.send_to(&ack, sizeof(ack), dest);
}

} // namespace nb
