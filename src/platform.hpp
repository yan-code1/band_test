#pragma once

// ── Version ─────────────────────────────────────────────────
#define NB_VERSION_MAJOR 1
#define NB_VERSION_MINOR 0
#define NB_VERSION_PATCH 0
#define NB_VERSION_STR   "1.0.0"

// ── Windows Target Version ──────────────────────────────────
#ifndef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WIN7  // Windows 7+
#endif

// ── Prevent Windows.h macro pollution ───────────────────────
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// ── Winsock2 must be included BEFORE windows.h ──────────────
#include <winsock2.h>
#include <ws2tcpip.h>   // getaddrinfo, etc.
#include <mswsock.h>    // TransmitFile, etc. (optional)
#include <windows.h>

#include <cstdint>
#include <string>
#include <system_error>

// ── Link pragma for ws2_32 (belt-and-suspenders with CMake) ─
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")

// ── Convenience ─────────────────────────────────────────────
namespace nb {

/// Wrap WSAStartup / WSACleanup with RAII.
struct WsaContext {
    WSADATA data{};

    WsaContext() {
        int rc = WSAStartup(MAKEWORD(2, 2), &data);
        if (rc != 0) {
            throw std::system_error(rc, std::system_category(),
                                    "WSAStartup failed");
        }
        if (LOBYTE(data.wVersion) != 2 || HIBYTE(data.wVersion) != 2) {
            WSACleanup();
            throw std::runtime_error("Winsock 2.2 not available");
        }
    }

    WsaContext(const WsaContext&) = delete;
    WsaContext& operator=(const WsaContext&) = delete;

    ~WsaContext() { WSACleanup(); }
};

} // namespace nb
