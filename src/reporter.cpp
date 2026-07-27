#include "reporter.hpp"
#include "platform.hpp"
#include <nlohmann/json.hpp>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cmath>

namespace nb {

// ── Formatting helpers ─────────────────────────────────────
static std::string format_bitrate(uint64_t bps) {
    if (bps >= 1'000'000'000)
        return std::to_string(bps / 1'000'000'000) + " Gbps";
    if (bps >= 1'000'000)
        return std::to_string(bps / 1'000'000) + " Mbps";
    if (bps >= 1'000)
        return std::to_string(bps / 1'000) + " Kbps";
    return std::to_string(bps) + " bps";
}

Reporter::Reporter(const Config& cfg)
    : json_mode_(cfg.json_output)
    , forceflush_(cfg.forceflush) {
    if (!cfg.logfile.empty()) {
        logfile_ = std::make_unique<std::ofstream>(cfg.logfile, std::ios::app);
    }
}

Reporter::~Reporter() {
    // If JSON mode was started but report_summary() was never called
    // (e.g., Ctrl+C during test), flush partial JSON data
    if (json_mode_ && json_started_ && !json_.contains("end")) {
        json_["end"] = nullptr;  // mark incomplete
        write_terminal(json_.dump(2) + "\n");
    }
}

void Reporter::report_start(const Config& cfg) {
    // ── Get current time ────────────────────────────────────
    std::time_t now = std::time(nullptr);
    char time_buf[64] = {};
#if defined(_MSC_VER)
    struct tm tm_buf;
    gmtime_s(&tm_buf, &now);
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
#else
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%SZ",
                  std::gmtime(&now));
#endif

    // ── Terminal header ─────────────────────────────────────
    if (!json_mode_) {
        std::ostringstream os;
        os << "Starting test: server " << cfg.server_host
           << ":" << cfg.port
           << ", bitrate " << format_bitrate(cfg.bitrate_bps)
           << ", duration=" << cfg.duration_sec << "s"
           << ", packet_len=" << cfg.packet_len << std::endl;
        os << std::string(75, '-') << std::endl;
        os << "[ ID] Interval        Transfer      Bitrate       Jitter   "
              "Lost/Total   Loss%  OoO" << std::endl;
        write_terminal(os.str());
    } else {
        // Build JSON start object
        json_["start"]["version"]     = NB_VERSION_STR;
        json_["start"]["server_host"] = cfg.server_host;
        json_["start"]["port"]        = cfg.port;
        json_["start"]["bitrate_bps"] = cfg.bitrate_bps;
        json_["start"]["duration_sec"] = cfg.duration_sec;
        json_["start"]["packet_len"]  = cfg.packet_len;
        json_["start"]["ipv6"]        = cfg.ipv6;
        json_["start"]["timestamp"]   = time_buf;
        json_["intervals"] = nlohmann::json::array();
        json_started_ = true;
    }
}

void Reporter::report_interval(const IntervalSnapshot& snap) {
    // ── Terminal line ───────────────────────────────────────
    if (!json_mode_) {
        std::ostringstream os;
        char line[256];
        std::snprintf(line, sizeof(line),
            "[%3u] %5.2f-%-5.2f sec %8.2f MBytes  %7.0f %-5s  %6.3fms  "
            "%4llu/%-5llu  %5.2f%%  %3u",
            snap.stream_id,
            snap.start_sec, snap.end_sec,
            static_cast<double>(snap.bytes) / (1024.0 * 1024.0),  // MBytes
            snap.bits_per_second < 1'000'000
                ? static_cast<double>(snap.bits_per_second) / 1000.0
                : static_cast<double>(snap.bits_per_second) / 1'000'000.0,
            snap.bits_per_second < 1'000'000 ? "Kbps" : "Mbps",
            snap.jitter_ms,
            static_cast<unsigned long long>(snap.lost_packets),
            static_cast<unsigned long long>(snap.total_packets),
            snap.lost_percent,
            snap.out_of_order);
        os << line << std::endl;
        write_terminal(os.str());
    } else if (json_started_) {
        // Append JSON interval
        nlohmann::json j;
        j["stream_id"]      = snap.stream_id;
        j["start_sec"]      = snap.start_sec;
        j["end_sec"]        = snap.end_sec;
        j["bytes"]          = snap.bytes;
        j["bits_per_second"] = snap.bits_per_second;
        j["jitter_ms"]      = snap.jitter_ms;
        j["lost_packets"]   = snap.lost_packets;
        j["total_packets"]  = snap.total_packets;
        j["lost_percent"]   = snap.lost_percent;
        j["out_of_order"]   = snap.out_of_order;
        json_["intervals"].push_back(std::move(j));
    }
}

void Reporter::report_summary(const StatsSummary& local,
                               const StatsSummary* server) {
    // ── Terminal ────────────────────────────────────────────
    if (!json_mode_) {
        std::ostringstream os;
        os << std::string(75, '-') << std::endl;

        char line[256];
        std::snprintf(line, sizeof(line),
            "[%3u] 0.00-%-5.2f sec %8.2f MBytes  %7.0f %-5s  %6.3fms  "
            "%4llu/%-5llu  %5.2f%%  %3u",
            1,
            local.duration_sec,
            static_cast<double>(local.bytes_received) / (1024.0 * 1024.0),
            local.bits_per_second < 1'000'000
                ? static_cast<double>(local.bits_per_second) / 1000.0
                : static_cast<double>(local.bits_per_second) / 1'000'000.0,
            local.bits_per_second < 1'000'000 ? "Kbps" : "Mbps",
            local.jitter_ms,
            static_cast<unsigned long long>(local.lost_packets),
            static_cast<unsigned long long>(local.total_packets),
            local.lost_percent,
            local.out_of_order);
        os << line << std::endl;

        if (server) {
            os << "Server Report:" << std::endl;
            std::snprintf(line, sizeof(line),
                "  Received: %llu/%llu packets (%.2f%%)\n"
                "  Bytes:    %.2f MBytes\n"
                "  Jitter:   %.3f ms (min=%.3f ms, max=%.3f ms)",
                static_cast<unsigned long long>(server->packets_received),
                static_cast<unsigned long long>(server->total_packets),
                100.0 - server->lost_percent,
                static_cast<double>(server->bytes_received) / (1024.0 * 1024.0),
                server->jitter_ms, server->jitter_min_ms, server->jitter_max_ms);
            os << line << std::endl;
        }

        os << std::string(75, '-') << std::endl;
        write_terminal(os.str());
    }

    // ── JSON output ────────────────────────────────────────
    if (json_mode_) {
        json_["end"]["duration_sec"]      = local.duration_sec;
        json_["end"]["bytes_received"]    = local.bytes_received;
        json_["end"]["packets_sent"]      = local.packets_sent;
        json_["end"]["packets_received"]  = local.packets_received;
        json_["end"]["bits_per_second"]   = local.bits_per_second;
        json_["end"]["jitter_ms"]         = local.jitter_ms;
        json_["end"]["jitter_min_ms"]     = local.jitter_min_ms;
        json_["end"]["jitter_max_ms"]     = local.jitter_max_ms;
        json_["end"]["lost_packets"]      = local.lost_packets;
        json_["end"]["total_packets"]     = local.total_packets;
        json_["end"]["lost_percent"]      = local.lost_percent;
        json_["end"]["out_of_order"]      = local.out_of_order;
        json_["end"]["duplicate_packets"] = local.duplicate_packets;

        if (server) {
            json_["server"]["duration_sec"]      = server->duration_sec;
            json_["server"]["bytes_received"]    = server->bytes_received;
            json_["server"]["packets_received"]  = server->packets_received;
            json_["server"]["packets_sent"]      = server->packets_sent;
            json_["server"]["bits_per_second"]   = server->bits_per_second;
            json_["server"]["jitter_ms"]         = server->jitter_ms;
            json_["server"]["jitter_min_ms"]     = server->jitter_min_ms;
            json_["server"]["jitter_max_ms"]     = server->jitter_max_ms;
            json_["server"]["lost_packets"]      = server->lost_packets;
            json_["server"]["total_packets"]     = server->total_packets;
            json_["server"]["lost_percent"]      = server->lost_percent;
            json_["server"]["out_of_order"]      = server->out_of_order;
            json_["server"]["duplicate_packets"] = server->duplicate_packets;
        }

        write_terminal(json_.dump(2) + "\n");
    }
}

void Reporter::report_error(const std::string& msg) {
    write_terminal("Error: " + msg + "\n");
}

void Reporter::report_info(const std::string& msg) {
    write_terminal("[Info] " + msg + "\n");
}

void Reporter::write_terminal(const std::string& line) {
    std::cout << line;
    if (logfile_) {
        *logfile_ << line;
        if (forceflush_) logfile_->flush();
    }
}

void Reporter::flush() {
    std::cout.flush();
    if (logfile_) logfile_->flush();
}

} // namespace nb
