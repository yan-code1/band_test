# nb — Windows UDP Network Benchmark Tool

`nb.exe` is a lightweight command-line network benchmarking tool for Windows, similar to [iPerf3](https://iperf.fr/). It measures UDP throughput, packet loss, and jitter between two machines.

## Features

- **Server mode** (`-s`) — receive UDP traffic and report statistics
- **Client mode** (`-c <host>`) — generate UDP traffic with controlled bandwidth
- **UDP only** — bandwidth, packet loss, jitter (RFC 3550), out-of-order detection
- **Adjustable bandwidth** — from bps to Gbps with `b/k/m/g` suffixes
- **Output** — human-readable terminal table **and/or** JSON (`-J`)
- **Graceful shutdown** — Ctrl+C produces partial results
- **Single executable** — no runtime dependencies (static CRT)

## Quick Start

```bash
# On the server machine:
nb.exe -s -p 5201

# On the client machine (10 Mbps, 30 seconds):
nb.exe -c 192.168.1.100 -p 5201 -b 10m -t 30

# JSON output (for scripts):
nb.exe -c 192.168.1.100 -J
```

## Usage

```
nb.exe <mode> [options]

Mode:
  -s, --server            Start server mode
  -c, --client <host>     Start client mode, connect to <host>

Client Options:
  -p, --port <port>       Server port (default: 5201)
  -b, --bitrate <rate>    Target bitrate (default: 1m, supports b/k/m/g)
  -l, --len <bytes>       Packet length incl. 24B header (default: 1470, range: 24~1472)
  -t, --time <sec>        Test duration (default: 10)
  -i, --interval <sec>    Report interval (default: 1)
  -4                     IPv4 mode (default)
  -6                     IPv6 mode
  --bind <host>          Bind to local address
  --tos <value>          Set IP_TOS/DSCP value

Server Options:
  -p, --port <port>       Listen port (default: 5201)
  -1                      One-shot mode (exit after one client)
  --idle-timeout <sec>    Client idle timeout (default: 5)

Common Options:
  -J, --json              JSON output
  --logfile <file>        Write output to file
  -V, --version           Show version
  -h, --help              Show help
```

## Examples

```bash
# 100 Mbps UDP test for 60 seconds, 1-second intervals
nb.exe -c server.example.com -b 100m -t 60 -i 1

# One-shot server (exit after single test)
nb.exe -s -p 5201 -1

# IPv6 test
nb.exe -s -6
nb.exe -c ::1 -6

# Log to file
nb.exe -c 192.168.1.100 -J --logfile results.json
```

## Build

### Prerequisites

- Windows 7+ with Visual Studio 2022 (or Build Tools)
- CMake 3.16+

### Build Steps

```bash
# Clone or download the source
cd band_test

# Configure
cmake -B build

# Build
cmake --build build

# The executable is at:
build/bin/nb.exe
```

### Dependencies

Header-only libraries (included in `third_party/`):
- [CLI11](https://github.com/CLIUtils/CLI11) — command-line argument parsing
- [nlohmann/json](https://github.com/nlohmann/json) — JSON generation/parsing

No additional installation required.

## Output Format

### Terminal

```
[ ID] Interval        Transfer      Bitrate       Jitter   Lost/Total   Loss%  OoO
[  1] 0.00-1.00 sec   1.25 MBytes   10.5 Mbps   0.123ms   23/ 1012    2.27%   0
[  1] 1.00-2.00 sec   1.24 MBytes   10.4 Mbps   0.098ms   18/ 1005    1.79%   1
-----------------------------------------------------------
[  1] 0.00-10.00 sec  12.5 MBytes   10.5 Mbps   0.112ms  210/10180    2.06%   2
Server Report:
  Received: 9970/10180 packets (97.94%)
  Bytes:    14.3 MBytes
  Jitter:   0.112 ms (min=0.041 ms, max=0.893 ms)
```

### JSON

```json
{
    "start": { "timestamp": "...", "version": "1.0.0", "system_info": {...} },
    "test_config": { "bitrate_bps": 10000000, "duration_sec": 10, ... },
    "intervals": [ { ... } ],
    "end": {
        "client_stats": { "bytes_sent": ..., "packets_sent": ... },
        "server_stats": { "bytes_received": ..., "jitter_ms": ..., "lost_percent": ... }
    }
}
```

## Further Reading

Detailed design documentation, protocol specification, state machines, and API reference:

- **`docs/developer-guide.md`** — Full developer guide with architecture, protocol spec, algorithms, module API, Windows platform notes, and debugging tips

## Limitations

- UDP only (no TCP/TLS support in v1.0)
- Single stream only (no parallel streams)
- Server handles one client at a time
- Best-effort pacing on Windows (~10% accuracy above 1 Gbps due to OS sleep limits)
- Jitter accuracy affected by clock drift (~25ppm between machines)

## License

MIT
