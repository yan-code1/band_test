---
severity: medium
status: completed
category: architecture
file: src/control_protocol.cpp
line: 129
---

# CR-011: ControlProtocol bypasses UdpSocket abstraction

**Severity:** Medium — should fix, non-blocking

## Problem

`send_result()` calls `::recvfrom(sock.native_handle(), ...)` directly instead of `sock.recv_from()`, and handles `WSAGetLastError()` directly. This bypasses the error translation in `UdpSocket::recv_from()`. It also calls `sock.set_recv_timeout()` as a side effect which may interfere with external timeout settings.

## Location

`src/control_protocol.cpp:129-145` — raw `recvfrom` and `WSAGetLastError()` calls

## Fix

Use `sock.recv_from()` and `sock.set_recv_timeout()` properly. Consider adding `recv_from_with_timeout()` to `UdpSocket` to avoid modifying the socket's persistent timeout.

## Verification

- Compile, run Result ACK test, verify ACK detection still works

### Effort: Small
