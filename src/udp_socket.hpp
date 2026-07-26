#pragma once

#include "platform.hpp"
#include "protocol_header.hpp"

#include <memory>
#include <string>
#include <chrono>

namespace nb {

/// RAII wrapper around a Winsock UDP socket.
class UdpSocket {
public:
    /// Create and bind to the given address/port.
    /// Address may be empty (any), "0.0.0.0", or "::".
    UdpSocket(bool ipv6, const std::string& bind_addr, uint16_t port);

    /// Create a socket for connecting to a remote host (client mode).
    static std::unique_ptr<UdpSocket> create_client(bool ipv6);

    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;

    // ── Operations ──────────────────────────────────────────
    void bind(const std::string& addr, uint16_t port);
    void connect(const sockaddr_storage& addr);
    void send_to(const uint8_t* data, size_t len, const sockaddr_storage& dest);
    int  recv_from(uint8_t* buf, size_t buf_size,
                   sockaddr_storage* src = nullptr);

    /// Set receive timeout (for non-blocking state checks).
    void set_recv_timeout(std::chrono::milliseconds ms);

    /// Set socket buffer sizes.
    void set_buffers(int recv_kb = 2048, int send_kb = 512);

    /// Check for async error (ICMP unreachable, etc.).
    /// Returns 0 if no error, or the error code.
    int check_error() const;

    /// Access underlying handle (for select/poll).
    SOCKET native_handle() const { return sock_; }

    /// Get the bound port (useful when port 0 was specified).
    uint16_t bound_port() const;

private:
    SOCKET sock_ = INVALID_SOCKET;

    /// Private default constructor — used by create_client().
    UdpSocket();

    void close() noexcept;
    static SOCKET create_socket(bool ipv6);
};

} // namespace nb
