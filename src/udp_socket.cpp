#include "udp_socket.hpp"
#include <cassert>

namespace nb {

UdpSocket::UdpSocket(bool ipv6, const std::string& bind_addr, uint16_t port)
    : sock_(create_socket(ipv6)) {
    if (sock_ == INVALID_SOCKET) {
        throw std::system_error(WSAGetLastError(), std::system_category(),
                                "socket() failed");
    }
    set_buffers();
    bind(bind_addr, port);
}

std::unique_ptr<UdpSocket> UdpSocket::create_client(bool ipv6) {
    SOCKET s = create_socket(ipv6);
    if (s == INVALID_SOCKET) {
        throw std::system_error(WSAGetLastError(), std::system_category(),
                                "socket() failed");
    }
    auto sock = std::unique_ptr<UdpSocket>(new UdpSocket);
    sock->sock_ = s;
    sock->set_buffers();
    return sock;
}

UdpSocket::UdpSocket() : sock_(INVALID_SOCKET) {}

UdpSocket::~UdpSocket() { close(); }

UdpSocket::UdpSocket(UdpSocket&& other) noexcept
    : sock_(other.sock_) {
    other.sock_ = INVALID_SOCKET;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        close();
        sock_ = other.sock_;
        other.sock_ = INVALID_SOCKET;
    }
    return *this;
}

void UdpSocket::close() noexcept {
    if (sock_ != INVALID_SOCKET) {
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
}

SOCKET UdpSocket::create_socket(bool ipv6) {
    return ::socket(ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

void UdpSocket::bind(const std::string& addr, uint16_t port) {
    // We need to know the family. Default to IPv4.
    // A proper implementation would parse addr or track family from creation.
    // For now, try both and keep the one that works.

    if (addr.empty() || addr == "0.0.0.0") {
        // IPv4 any
        sockaddr_in sin{};
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);
        sin.sin_addr.s_addr = INADDR_ANY;
        if (::bind(sock_, reinterpret_cast<sockaddr*>(&sin), sizeof(sin)) == 0) {
            return;
        }
        // Fall through to try IPv6
    }

    // IPv6 any (or address parse failed above)
    sockaddr_in6 sin6{};
    sin6.sin6_family = AF_INET6;
    sin6.sin6_port = htons(port);
    sin6.sin6_addr = in6addr_any;
    if (::bind(sock_, reinterpret_cast<sockaddr*>(&sin6), sizeof(sin6)) != 0) {
        throw std::system_error(WSAGetLastError(), std::system_category(),
                                "bind() failed");
    }
}

void UdpSocket::set_buffers(int recv_kb, int send_kb) {
    int rcvbuf = recv_kb * 1024;
    setsockopt(sock_, SOL_SOCKET, SO_RCVBUF,
               reinterpret_cast<char*>(&rcvbuf), sizeof(rcvbuf));
    int sndbuf = send_kb * 1024;
    setsockopt(sock_, SOL_SOCKET, SO_SNDBUF,
               reinterpret_cast<char*>(&sndbuf), sizeof(sndbuf));
}

void UdpSocket::set_recv_timeout(std::chrono::milliseconds ms) {
    DWORD timeout = static_cast<DWORD>(ms.count());
    setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<char*>(&timeout), sizeof(timeout));
}

void UdpSocket::send_to(const uint8_t* data, size_t len,
                         const sockaddr_storage& dest) {
    int rc;
    if (dest.ss_family == AF_INET) {
        rc = ::sendto(sock_, reinterpret_cast<const char*>(data),
                      static_cast<int>(len), 0,
                      reinterpret_cast<const sockaddr*>(&dest),
                      sizeof(sockaddr_in));
    } else {
        rc = ::sendto(sock_, reinterpret_cast<const char*>(data),
                      static_cast<int>(len), 0,
                      reinterpret_cast<const sockaddr*>(&dest),
                      sizeof(sockaddr_in6));
    }
    if (rc == SOCKET_ERROR) {
        throw std::system_error(WSAGetLastError(), std::system_category(),
                                "sendto() failed");
    }
}

int UdpSocket::recv_from(uint8_t* buf, size_t buf_size,
                          sockaddr_storage* src) {
    sockaddr_storage from{};
    int fromlen = sizeof(from);
    int rc = ::recvfrom(sock_, reinterpret_cast<char*>(buf),
                        static_cast<int>(buf_size), 0,
                        reinterpret_cast<sockaddr*>(&from), &fromlen);
    if (rc == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAETIMEDOUT) return -1;
        throw std::system_error(err, std::system_category(),
                                "recvfrom() failed");
    }
    if (src) *src = from;
    return rc;
}

void UdpSocket::connect(const sockaddr_storage& addr) {
    int rc;
    if (addr.ss_family == AF_INET) {
        rc = ::connect(sock_, reinterpret_cast<const sockaddr*>(&addr),
                       sizeof(sockaddr_in));
    } else {
        rc = ::connect(sock_, reinterpret_cast<const sockaddr*>(&addr),
                       sizeof(sockaddr_in6));
    }
    if (rc == SOCKET_ERROR) {
        throw std::system_error(WSAGetLastError(), std::system_category(),
                                "connect() failed");
    }
}

uint16_t UdpSocket::bound_port() const {
    sockaddr_storage addr{};
    int len = sizeof(addr);
    if (getsockname(sock_, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
        return 0;
    }
    if (addr.ss_family == AF_INET) {
        return ntohs(reinterpret_cast<sockaddr_in*>(&addr)->sin_port);
    }
    return ntohs(reinterpret_cast<sockaddr_in6*>(&addr)->sin6_port);
}

int UdpSocket::check_error() const {
    int error = 0;
    socklen_t len = sizeof(error);
    getsockopt(sock_, SOL_SOCKET, SO_ERROR,
               reinterpret_cast<char*>(&error), &len);
    return error;
}

} // namespace nb
