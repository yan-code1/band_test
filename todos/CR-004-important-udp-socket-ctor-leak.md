---
severity: important
status: completed
category: correctness
file: src/udp_socket.cpp
line: 6
---

# CR-004: UdpSocket constructor throw leaks SOCKET handle

**Severity:** Important — should fix before merge

## Problem

`UdpSocket(bool ipv6, const std::string& bind_addr, uint16_t port)` creates a raw SOCKET in the constructor body, then calls `set_buffers()` and `bind()`. If `bind()` or `set_buffers()` throws (e.g., port unavailable), `sock_` is leaked because the destructor is not called for a partially-constructed object.

## Location

`src/udp_socket.cpp:6-14`

```cpp
UdpSocket::UdpSocket(bool ipv6, const std::string& bind_addr, uint16_t port)
    : sock_(create_socket(ipv6)) {
    if (sock_ == INVALID_SOCKET) { throw ...; }
    set_buffers();     // ← if throws, sock_ leaks
    bind(bind_addr, port);  // ← if throws, sock_ leaks
}
```

## Fix

Add a scope guard:

```cpp
UdpSocket::UdpSocket(bool ipv6, const std::string& bind_addr, uint16_t port)
    : sock_(create_socket(ipv6)) {
    if (sock_ == INVALID_SOCKET) { throw ...; }
    // scope guard: close on exception
    auto guard = [&](SOCKET* s) { if (*s != INVALID_SOCKET) closesocket(*s); };
    // ... or use a unique_ptr deleter
}
```

Alternatively, follow the `create_client()` factory pattern: create the socket empty, then call methods separately.

## Verification

- Try `bind()` to port 0 or invalid address, verify no handle leak (use Process Explorer)

### Effort: Small
