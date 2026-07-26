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

/// Convert Windows socket error to std::error_code.
inline std::error_code last_socket_error() noexcept {
    return std::error_code(WSAGetLastError(), std::system_category());
}

/// Human-readable socket error message.
inline std::string socket_error_string(int ec) {
    wchar_t* buf = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                       FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, ec, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
    std::string result;
    if (buf) {
        int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1,
                                      nullptr, 0, nullptr, nullptr);
        result.resize(len - 1);
        WideCharToMultiByte(CP_UTF8, 0, buf, -1, result.data(), len,
                            nullptr, nullptr);
        LocalFree(buf);
    }
    return result;
}

} // namespace nb
