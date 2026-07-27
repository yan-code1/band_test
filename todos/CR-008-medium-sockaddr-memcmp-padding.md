---
severity: medium
status: completed
category: correctness
file: src/server_handler.cpp
line: 97
---

# CR-008: sockaddr_storage comparison includes uninitialized padding

**Severity:** Medium — should fix, non-blocking

## Problem

`sockaddr_storage from{}` and `client_addr` are compared via `memcmp(&from, &client_addr, sizeof(from))` which compares all 128 bytes including padding. On the hot receive path, this is also extra memory traffic.

## Location

`src/server_handler.cpp:97`

```cpp
if (client_addr_valid &&
    std::memcmp(&from, &client_addr, sizeof(from)) != 0) {
    continue;
}
```

## Fix

Use family-specific comparison of `ss_family` + address + port (compare `sockaddr_in` or `sockaddr_in6` sizes).

## Verification

- Test with both IPv4 and IPv6 clients, verify no false rejections

### Effort: Small
