// nb.exe — Windows UDP Network Benchmark Tool
//
// Usage:  nb.exe -s               (server mode)
//         nb.exe -c <host> -b 10m (client mode)
//
// See --help for full options.

#include "platform.hpp"
#include "argument_parser.hpp"
#include "console_handler.hpp"
#include "server_handler.hpp"
#include "client_handler.hpp"

#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    // ── Initialize Winsock (RAII — cleans up on scope exit) ─
    std::unique_ptr<nb::WsaContext> wsa;
    try {
        wsa = std::make_unique<nb::WsaContext>();
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << std::endl;
        return 1;
    }

    // ── Register Ctrl+C handler ─────────────────────────────
    nb::register_console_handler();

    // ── Parse arguments ─────────────────────────────────────
    auto cfg = nb::parse_args(argc, argv);
    if (!cfg) {
        return 1;  // error already printed by parse_args
    }

    // ── Dispatch ────────────────────────────────────────────
    int exit_code = 0;

    try {
        switch (cfg->mode) {
        case nb::Config::Mode::SERVER:
            nb::run_server(*cfg);
            break;
        case nb::Config::Mode::CLIENT:
            nb::run_client(*cfg);
            break;
        }
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << std::endl;
        exit_code = 1;
    }

    return exit_code;
}
