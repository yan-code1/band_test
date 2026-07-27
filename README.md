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
-------------------------------------------------------------------------------
[  1] 0.00-1.00 sec   1.25 MBytes   10.5 Mbps   0.123ms   23/ 1012    2.27%   0
[  1] 1.00-2.00 sec   1.24 MBytes   10.4 Mbps   0.098ms   18/ 1005    1.79%   1
-------------------------------------------------------------------------------
[  1] 0.00-10.00 sec  12.5 MBytes   10.5 Mbps   0.112ms    0/    0     --%   2  sender
[  1] 0.00-10.00 sec  12.3 MBytes   10.4 Mbps   0.112ms  210/10180    2.06%   2  receiver
-------------------------------------------------------------------------------
Lost: 210/10180 (2.06%)
```

### JSON

```json
{
    "start": {
        "version": "1.0.0",
        "server_host": "192.168.1.100",
        "port": 5201,
        "bitrate_bps": 10000000,
        "duration_sec": 10,
        "packet_len": 1470,
        "ipv6": false,
        "timestamp": "2026-07-27T10:30:00Z"
    },
    "intervals": [
        {
            "stream_id": 1,
            "start_sec": 0.0,
            "end_sec": 1.0,
            "bytes": 1310720,
            "bits_per_second": 10485760,
            "jitter_ms": 0.123,
            "lost_packets": 23,
            "total_packets": 1012,
            "lost_percent": 2.27,
            "out_of_order": 0
        }
    ],
    "end": {
        "duration_sec": 10.0,
        "bytes_sent": 14961960,
        "bytes_received": 14655900,
        "packets_sent": 10180,
        "packets_received": 9970,
        "bits_per_second": 10240000,
        "jitter_ms": 0.112,
        "jitter_min_ms": 0.041,
        "jitter_max_ms": 0.893,
        "lost_packets": 210,
        "total_packets": 10180,
        "lost_percent": 2.06,
        "out_of_order": 2,
        "duplicate_packets": 0
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
- Best-effort pacing on Windows (hybrid sleep+spin, ~1-2% accuracy up to 10 Gbps for large packets). High bitrates with small packets or tight intervals consume close to 100% of one CPU core for spin-wait
- Jitter accuracy affected by clock drift (~25ppm between machines)
- No sub-2ms sleep accuracy: uses spin-wait below 2ms, consuming full CPU core during active measurement

## License

MIT
