#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>
#include <cstring>

namespace nb {

// ── Custom Protocol Header (24 bytes, wire format) ──────────

#pragma pack(push, 1)
struct ProtocolHeader {
    uint32_t magic;         // 0x4E424E54 = "NBNT"
    uint8_t  version;       // 0x01
    uint8_t  msg_type;      // 0=data, 1=start, 2=finish, 3=result
    uint16_t flags;         // reserved, set to 0
    uint32_t packet_id;     // sequence number (1+ for data, 0 for control)
    uint32_t timestamp_sec; // Unix timestamp (UTC seconds)
    uint32_t timestamp_nsec;// Nanoseconds within the second
    uint32_t total_length;  // UDP payload length incl. header (0 for start/finish)
};
#pragma pack(pop)

static_assert(sizeof(ProtocolHeader) == 24,
              "ProtocolHeader must be exactly 24 bytes");

// ── Message Types ───────────────────────────────────────────
enum MsgType : uint8_t {
    MSG_DATA   = 0,
    MSG_START  = 1,
    MSG_FINISH = 2,
    MSG_RESULT = 3,
};

// ── Magic Number ────────────────────────────────────────────
constexpr uint32_t NB_MAGIC = 0x4E424E54;  // "NBNT" little-endian
constexpr uint8_t  NB_VERSION = 0x01;
constexpr size_t   HEADER_SIZE = sizeof(ProtocolHeader);

// Maximum safe UDP payload size without IP fragmentation
// (Ethernet MTU 1500 - IP 20 - UDP 8 = 1472)
constexpr size_t MAX_UDP_PAYLOAD = 1472;

// Default total packet length (24 header + 1446 payload)
constexpr size_t DEFAULT_PACKET_LEN = 1470;

// Min length = just the header
constexpr size_t MIN_PACKET_LEN = HEADER_SIZE;

// ── Serialization ───────────────────────────────────────────

inline void header_to_wire(const ProtocolHeader& hdr, uint8_t* buf) {
    std::memcpy(buf, &hdr, HEADER_SIZE);
}

inline std::optional<ProtocolHeader> header_from_wire(const uint8_t* buf,
                                                      size_t len) {
    if (len < HEADER_SIZE) return std::nullopt;

    ProtocolHeader hdr;
    std::memcpy(&hdr, buf, HEADER_SIZE);

    if (hdr.magic != NB_MAGIC)     return std::nullopt;
    if (hdr.version != NB_VERSION) return std::nullopt;

    return hdr;
}

// ── Builder Helpers ─────────────────────────────────────────

inline ProtocolHeader make_data_header(uint32_t packet_id,
                                       uint32_t payload_size,
                                       uint64_t timestamp_ns) {
    ProtocolHeader hdr{};
    hdr.magic        = NB_MAGIC;
    hdr.version      = NB_VERSION;
    hdr.msg_type     = MSG_DATA;
    hdr.flags        = 0;
    hdr.packet_id    = packet_id;
    hdr.timestamp_sec = static_cast<uint32_t>(timestamp_ns / 1'000'000'000);
    hdr.timestamp_nsec = static_cast<uint32_t>(timestamp_ns % 1'000'000'000);
    hdr.total_length = payload_size + HEADER_SIZE;
    return hdr;
}

inline ProtocolHeader make_start_header(uint32_t /*duration_sec*/) {
    ProtocolHeader hdr{};
    hdr.magic        = NB_MAGIC;
    hdr.version      = NB_VERSION;
    hdr.msg_type     = MSG_START;
    hdr.flags        = 0;
    hdr.packet_id    = 0;
    hdr.timestamp_sec = 0;
    hdr.timestamp_nsec = 0;
    hdr.total_length = 0;
    // duration is carried in 8-byte payload following header
    return hdr;
}

inline ProtocolHeader make_finish_header() {
    ProtocolHeader hdr{};
    hdr.magic        = NB_MAGIC;
    hdr.version      = NB_VERSION;
    hdr.msg_type     = MSG_FINISH;
    hdr.flags        = 0;
    hdr.packet_id    = 0;
    hdr.timestamp_sec = 0;
    hdr.timestamp_nsec = 0;
    hdr.total_length = 0;
    return hdr;
}

inline ProtocolHeader make_result_header(uint32_t payload_size) {
    ProtocolHeader hdr{};
    hdr.magic        = NB_MAGIC;
    hdr.version      = NB_VERSION;
    hdr.msg_type     = MSG_RESULT;
    hdr.flags        = 0;
    hdr.packet_id    = 0;
    hdr.timestamp_sec = 0;
    hdr.timestamp_nsec = 0;
    hdr.total_length = payload_size + HEADER_SIZE;
    return hdr;
}

} // namespace nb
