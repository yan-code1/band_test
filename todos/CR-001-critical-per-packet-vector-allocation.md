---
severity: critical
status: completed
category: performance
file: src/client_handler.cpp
line: 131
---

# CR-001: Per-packet heap vector allocation in hot send loop

**Severity:** Critical — BLOCKS MERGE

## Problem

Every send iteration allocates a fresh `std::vector<uint8_t> pkt(cfg.packet_len)` (~1470 bytes on heap), copies the header, sends, then immediately destroys (free). At 1 Gbps with 1470B packets (~85K pkts/s), this produces ~125 MB/s of heap churn. At 10 Gbps, ~1.25 GB/s. This directly limits achievable throughput.

## Location

`src/client_handler.cpp:131` — inside the sender `while` loop

```cpp
std::vector<uint8_t> pkt(cfg.packet_len);   // ← per-packet allocation
header_to_wire(hdr, pkt.data());
sock->send_to(pkt.data(), pkt.size(), server_addr);
```

Note: line 69 already has `send_buf` allocated for reuse but it is dead code.

## Fix

Move allocation outside the loop, reuse buffer:

```cpp
std::vector<uint8_t> pkt(cfg.packet_len, 'D');  // allocated once
while (!g_shutdown && !test_completed) {
    auto hdr = make_data_header(packet_id, ...);
    header_to_wire(hdr, pkt.data());            // overwrite first 24 bytes
    // ... send ...
}
```

Remove unused `send_buf` on line 69.

## Verification

- Build and run `-b 1000m -t 5`, verify throughput is not degraded
- Compare heap allocation rate with/without fix (e.g., ETW traces)

### Effort: Small
