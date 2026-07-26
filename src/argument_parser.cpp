#include "argument_parser.hpp"
#include "protocol_header.hpp"
#include "platform.hpp"

#include <CLI/CLI.hpp>
#include <iostream>
#include <cstdlib>

namespace nb {

// ── Bitrate suffix parsing (e.g., "10m", "1g") ─────────────
static uint64_t parse_bitrate(const std::string& s) {
    if (s.empty()) return 0;

    std::string num_part = s;
    uint64_t multiplier = 1;

    char suffix = num_part.back();
    switch (suffix) {
    case 'b': case 'B': multiplier = 1;          num_part.pop_back(); break;
    case 'k': case 'K': multiplier = 1000;       num_part.pop_back(); break;
    case 'm': case 'M': multiplier = 1'000'000;  num_part.pop_back(); break;
    case 'g': case 'G': multiplier = 1'000'000'000; num_part.pop_back(); break;
    default: break;
    }

    char* end = nullptr;
    double val = std::strtod(num_part.c_str(), &end);
    if (end == num_part.c_str() || *end != '\0') return 0;
    if (val <= 0) return 0;

    return static_cast<uint64_t>(val * multiplier);
}

// ── Parse ───────────────────────────────────────────────────
std::optional<Config> parse_args(int argc, char* argv[]) {
    Config cfg;
    std::string bitrate_str = "1m";

    CLI::App app{"nb — Windows UDP Network Benchmark Tool v" NB_VERSION_STR};

    // ── Mode flags ─────────────────────────────────────────
    bool server_mode = false;
    std::string client_host;

    app.add_flag("-s,--server", server_mode,
                 "启动服务端模式");
    app.add_option("-c,--client", client_host,
                   "启动客户端模式，连接指定服务端");

    // ── Common options ─────────────────────────────────────
    app.add_option("-p,--port", cfg.port,
                   "服务端端口号 (默认: 5201)");
    app.add_flag("-4", [&](size_t) { cfg.ipv6 = false; },
                 "IPv4 模式 (默认)");
    app.add_flag("-6", [&](size_t) { cfg.ipv6 = true; },
                 "IPv6 模式");
    app.add_flag("-J,--json", cfg.json_output,
                 "以 JSON 格式输出");
    app.add_option("--logfile", cfg.logfile,
                   "将输出同时写入文件");
    app.add_flag("--forceflush", cfg.forceflush,
                 "实时刷新日志 (无缓冲)");
    app.add_flag("-V,--version", cfg.version_flag,
                 "显示版本信息");

    // ── Client options ─────────────────────────────────────
    app.add_option("-b,--bitrate", bitrate_str,
                   "目标发送带宽 (默认: 1m, 支持 b/k/m/g 后缀)");
    app.add_option("-l,--len", cfg.packet_len,
                   "报文总长度(含24B协议头) (默认: 1470, 范围: 24~1472)");
    app.add_option("-t,--time", cfg.duration_sec,
                   "测试持续时间 (秒, 默认: 10)");
    app.add_option("-i,--interval", cfg.interval_sec,
                   "汇报间隔 (秒, 默认: 1)");
    app.add_option("--bind", cfg.bind_addr,
                   "绑定本地地址 (多网卡场景)");

    // ── Server options ─────────────────────────────────────
    app.add_flag("-1", cfg.single_shot,
                 "单次模式：处理一个客户端后退出");
    app.add_option("--idle-timeout", cfg.idle_timeout_sec,
                   "客户端无数据超时时间 (秒, 默认: 5)");

    // ── Parse ───────────────────────────────────────────────
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError&) {
        // app.exit() prints help/error message to appropriate stream
        std::cerr << app.help() << std::endl;
        return std::nullopt;
    }

    // ── Version shortcut ────────────────────────────────────
    if (cfg.version_flag) {
        std::cout << "nb v" << NB_VERSION_STR << std::endl;
        std::exit(0);
    }

    // ── Mode validation ─────────────────────────────────────
    if (server_mode && !client_host.empty()) {
        std::cerr << "Error: 不能同时指定 -s 和 -c" << std::endl;
        return std::nullopt;
    }
    if (!server_mode && client_host.empty()) {
        std::cerr << "Error: 请指定 -s (服务端) 或 -c <host> (客户端)"
                  << std::endl;
        return std::nullopt;
    }

    if (server_mode) {
        cfg.mode = Config::Mode::SERVER;
    } else {
        cfg.mode = Config::Mode::CLIENT;
        cfg.server_host = client_host;
    }

    // ── Port validation ─────────────────────────────────────
    if (cfg.port < 1) {
        std::cerr << "Error: 端口号必须 > 0 (got " << cfg.port << ")"
                  << std::endl;
        return std::nullopt;
    }

    // ── Bitrate parsing ─────────────────────────────────────
    cfg.bitrate_bps = parse_bitrate(bitrate_str);
    if (cfg.bitrate_bps == 0) {
        std::cerr << "Error: 无效的带宽值 '" << bitrate_str << "'"
                  << std::endl;
        return std::nullopt;
    }

    // ── Packet length validation ────────────────────────────
    if (cfg.packet_len < MIN_PACKET_LEN ||
        cfg.packet_len > MAX_UDP_PAYLOAD) {
        std::cerr << "Error: 报文长度必须在 "
                  << MIN_PACKET_LEN << "~" << MAX_UDP_PAYLOAD
                  << " 之间 (got " << cfg.packet_len << ")"
                  << std::endl;
        return std::nullopt;
    }

    // ── Duration validation ─────────────────────────────────
    if (cfg.duration_sec < 1) {
        std::cerr << "Error: 测试时间必须 ≥ 1 秒 (got "
                  << cfg.duration_sec << ")" << std::endl;
        return std::nullopt;
    }

    // ── Interval clamping ───────────────────────────────────
    if (cfg.interval_sec > cfg.duration_sec) {
        cfg.interval_sec = cfg.duration_sec;
    }

    // ── Idle timeout validation ─────────────────────────────
    if (cfg.idle_timeout_sec < 1) {
        cfg.idle_timeout_sec = 5;
    }

    return cfg;
}

} // namespace nb
