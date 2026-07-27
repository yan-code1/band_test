---
severity: minor
status: completed
category: cleanup
file: multiple
line: 0
---

# CR-014: Dead code cleanup (multi-file)

**Severity:** Minor — optional

## Items to clean up

1. **`client_handler.cpp:69`** — `send_buf` allocated but never used (dead after CR-001 fix)
2. **`reporter.cpp:243-246`** — `flush()` method defined but never called
3. **`platform.hpp:62-84`** — `last_socket_error()` and `socket_error_string()` defined but never called
4. **`udp_socket.cpp:32-44`** — Move constructor and assignment operator never used
5. **`protocol_header.hpp:17`** — `flags` field documents `bit0=reverse(保留)` for feature that doesn't exist
6. **`client_handler.cpp:152`** — `WSAEWOULDBLOCK` check on blocking socket (dead condition)

## Effort: Medium (scattered across files)
