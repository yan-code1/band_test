#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <chrono>

namespace nb {

/// Parsed configuration from command-line arguments.
struct Config {
    // ── Mode ────────────────────────────────────────────────
    enum class Mode { SERVER, CLIENT };
    Mode mode = Mode::SERVER;

    // ── Network ─────────────────────────────────────────────
    std::string server_host;           // client mode only
    uint16_t    port = 5201;
    bool        ipv6 = false;

    // ── Traffic ─────────────────────────────────────────────
    uint64_t bitrate_bps = 1'000'000;  // default 1 Mbps
    uint32_t packet_len  = 1470;       // total UDP payload (incl. header)
    uint32_t duration_sec = 10;

    // ── Reporting ───────────────────────────────────────────
    uint32_t interval_sec = 1;
    bool     json_output  = false;
    std::string logfile;               // empty = no logfile
    bool     forceflush   = false;

    // ── Server ──────────────────────────────────────────────
    bool     single_shot = false;      // -1: exit after one test
    uint32_t idle_timeout_sec = 5;

    // ── Advanced ────────────────────────────────────────────
    std::string bind_addr;             // empty = any
    bool     version_flag = false;
    bool     help_flag    = false;
};

/// Parse command-line arguments. Returns nullopt on error (prints to stderr).
std::optional<Config> parse_args(int argc, char* argv[]);

} // namespace nb
