# nb — 开发指南

## 项目概述

`nb`（Network Benchmark）是一个 Windows 平台的命令行网络测速工具，功能类似 iperf3。它通过 UDP 协议测量两点间的带宽、丢包率、抖动等网络性能指标。

**特点：**
- 单个可执行文件 `nb.exe`，无外部运行依赖
- 服务端/客户端双模式
- 应用层带宽控制（混合 sleep + spin-wait + burst）
- 自定义 24 字节二进制协议头
- 终端文本 + JSON 双输出格式
- UDP 纯数据报模式（支持 IPv4 + IPv6）

---

## 目录结构

```
band_test/
├── CMakeLists.txt              # CMake 构建系统
├── README.md                   # 用户使用文档
├── src/
│   ├── main.cpp                # 入口点
│   ├── platform.hpp            # Windows 平台宏 & WsaContext
│   ├── argument_parser.cpp/.hpp # 命令行参数解析
│   ├── protocol_header.hpp     # 24B 协议头定义 & 序列化
│   ├── udp_socket.cpp/.hpp     # Winsock UDP RAII 封装
│   ├── server_handler.cpp/.hpp # 服务端状态机
│   ├── client_handler.cpp/.hpp # 客户端双线程引擎
│   ├── pacer.cpp/.hpp          # 带宽控制器
│   ├── stats_collector.cpp/.hpp # 统计收集 & 计算
│   ├── control_protocol.cpp/.hpp # Start/Finish/Result 协议
│   ├── reporter.cpp/.hpp       # 输出格式化 (文本+JSON)
│   └── console_handler.cpp/.hpp # Ctrl+C 处理
├── third_party/
│   ├── CLI/CLI.hpp             # CLI11 v2.2.0 (15 headers)
│   └── nlohmann/json.hpp       # JSON for Modern C++ v3.11.2
└── docs/
    └── developer-guide.md      # 本文档
```

---

## 构建指南

### 环境要求

- **OS:** Windows 7+（编译需要 Windows）
- **编译器:** Visual Studio 2022 或 Build Tools 2022（含 C++ 工作负载）
- **CMake:** 3.16+

### 构建步骤

```bash
# 克隆/下载源码后
cd band_test
cmake -B build          # 配置
cmake --build build     # 编译
# 输出: build/bin/Debug/nb.exe
```

CMake 会自动从 `third_party/` 引用头文件依赖，无需额外下载。

### 构建说明

| CMake 选项 | 说明 |
|-----------|------|
| `-DCMAKE_BUILD_TYPE=Release` | 发布模式（优化开启） |
| 默认 | Debug 模式（含调试符号） |

CMakeLists.txt 关键配置：
- C++20 标准
- MSVC 静态运行时链接（`MultiThreaded`），无需 vcredist
- `/W4 /WX` 警告即错误
- `/utf-8` 编码支持中文注释
- `ws2_32.lib` 链接

---

## 架构设计

### 总体架构

```
┌─────────────────────┐         ┌─────────────────────┐
│      Client         │         │       Server        │
│  ┌───────────────┐  │  UDP   │  ┌───────────────┐  │
│  │ Sender Thread │──┼────────┼─>│ Main Thread   │  │
│  │ (paced send)  │  │        │  │ (recvfrom)    │  │
│  └───────────────┘  │        │  └───────┬───────┘  │
│  ┌───────────────┐  │        │          │          │
│  │ Receiver Thd  │<─┼────────┼──────────┘          │
│  │ (wait Result) │  │        │                     │
│  └───────────────┘  │        │                     │
└─────────────────────┘        └─────────────────────┘
```

### 协议交互流程

```
Client                              Server
  |                                   |
  |─── [Start] msg_type=1 ──────────>|  重置统计
  |─── [Data]  msg_type=0 ──────────>|  记录数据
  |─── [Data]  msg_type=0 ──────────>|
  |─── ...                           |
  |─── [Finish] msg_type=2 ─────────>|  计算统计
  |<── [Result] msg_type=3 ──────────|  返回结果
  |─── [ACK]   空数据包 ────────────>|  确认
```

### 线程模型

**服务端: 单线程**
```
Main Thread:
  while (!g_shutdown):
    LISTENING: recvfrom(timeout) → 收到 Start → RECEIVING
    RECEIVING: recvfrom(timeout) → 收集数据包 → Finish/超时 → REPORTING
    REPORTING: finalize() → send_result() → IDLE
    IDLE: -1模式 → EXIT / 否则 → LISTENING
```

**客户端: 双线程**
```
Sender Thread:
  send Start → for (duration): wait_until() → sendto() → send Finish → 退出

Receiver Thread:
  select() → recvfrom() → 收到 Result → 解析 → 保存

主线程:
  定时报告 interval → join Sender → wait Receiver → report_summary()
```

---

## 协议规范

### 数据报头 (24 字节)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          magic_number  (0x4E424E54)          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  version(8)  |  msg_type(8) |           flags(16)            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           packet_id                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         timestamp_sec                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         timestamp_nsec                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          total_length                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                            payload ...                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### 字段说明

| 字段 | 大小 | 说明 |
|------|------|------|
| `magic` | uint32 | 固定 `0x4E424E54` ("NBNT")，用于识别合法数据包 |
| `version` | uint8 | 协议版本，当前 = `0x01` |
| `msg_type` | uint8 | `0`=数据, `1`=开始, `2`=结束, `3`=结果 |
| `flags` | uint16 | 预留标志位 |
| `packet_id` | uint32 | 数据包序列号（从 1 递增），控制包 = 0 |
| `timestamp_sec` | uint32 | 发送端 Unix 时间戳（秒） |
| `timestamp_nsec` | uint32 | 发送端时间戳（纳秒） |
| `total_length` | uint32 | 当前数据报总长度（含 24B 头），控制包 = 0 |
| payload | 变长 | 数据载荷 |

**magic 值 `0x4E424E54` 对应 ASCII `"NBNT"`（Network Benchmark Tool）。**

### 消息类型

| 类型 | 值 | 方向 | payload | 说明 |
|------|-----|------|---------|------|
| DATA | 0 | Client→Server | 用户数据 | 带宽测试数据包 |
| START | 1 | Client→Server | 8B: duration_sec + reserved | 通知开始测试 |
| FINISH | 2 | Client→Server | 8B: total_packets (uint64_t) | 通知结束发送，内嵌发包总数 |
| RESULT | 3 | Server→Client | JSON(≤1024B) | 返回统计结果 |

### Start Payload (8 字节)

```
Offset  Size  Field        Description
0       4     duration_sec 测试总时长（秒）
4       4     reserved     预留（全 0）
```

### Result Payload (JSON, ≤1024 字节)

```json
{
    "duration_sec": 5.0,
    "bytes_received": 5000000,
    "packets_received": 3401,
    "packets_sent": 3401,
    "bits_per_second": 10000000,
    "jitter_ms": 0.892,
    "jitter_min_ms": 0.0,
    "jitter_max_ms": 1.841,
    "lost_packets": 0,
    "total_packets": 3401,
    "lost_percent": 0.0,
    "out_of_order": 0,
    "duplicate_packets": 0
}
```

注意：服务端向客户端发送的 Result JSON 字段全部来自 `StatsSummary`。时长`duration_sec` 服务端以首尾包实际时间差计算，客户端本地汇报时使用 `-t` 指定值。

---

## 核心算法

### 带宽控制 (Pacer)

Windows `sleep_for()` 精度仅 ~1ms，而 10Gbps 下包间隔仅 ~1.18μs。采用混合模式：

```
wait_until(target):
  delta = target - now

  if delta > 2ms:
    sleep_for(delta - 100μs)     // 粗粒度等待
  while now < target:
    _mm_pause()                   // 微秒级 spin-wait

burst 模式:
  if packet_interval < 2ms:
    burst_size = ceil(2ms / interval)
    每次 wait_until 后连续发送 burst_size 个包
```

### 抖动计算 (RFC 3550)

```
D(i, j) = (R_j - S_j) - (R_i - S_i)    // 延迟差
J(i)    = J(i-1) + (|D(i-1, i)| - J(i-1)) / 16   // 指数平滑移动平均
```

- `S_i` = 报文 i 的发送时间戳（客户端时钟）
- `R_i` = 报文 i 的接收时间戳（服务端时钟）
- 差分公式自动消除固定时钟偏移
- 无法消除时钟漂移（~25ppm = 30s 测试 ~0.75ms 误差），但抖动是 ms 级指标，可接受

### 丢包率

```
total_packets > 0（从 Finish 消息获取客户端真实发包数）
→ 丢包率 = (total_packets - packets_received) / total_packets × 100%
```

- 客户端在 **Finish 消息**的 payload 中嵌入实际发包总数（8 字节 `uint64_t`）
- 服务端收到 Finish 后调用 `set_sender_packets()`，`finalize()` 据此精确计算丢包
- 如果 `total_packets == 0`（未收到 Finish 消息），单独报 `--/--` 表示丢包未知
- 乱序检测：packet_id 非递增时计数
- 重复包检测：`std::set` 滑动窗口追踪最近 100K 个 packet_id，超出窗口的 ID 被裁剪以控制内存

### 带宽计算

```
带宽(bps) = 总接收字节数 × 8 / 测试时长(秒)
```

发送端和接收端独立报告带宽（差异反映丢包）。

---

## 命令行接口

### 完整参数

```
nb.exe <mode> [options]

Mode:
  -s, --server           启动服务端模式
  -c, --client <host>    启动客户端模式，连接指定服务端

Client Options:
  -p, --port <port>      服务端端口号 (默认: 5201)
  -b, --bitrate <rate>   目标发送带宽 (默认: 1m, 支持 b/k/m/g 后缀)
  -l, --len <bytes>      报文总长度含24B协议头 (默认: 1470, 范围: 24~1472)
  -t, --time <sec>       测试持续时间 (默认: 10 秒, 最小 1 秒)
  -i, --interval <sec>   汇报间隔 (默认: 1 秒, 自动 ≤ -t)
  -4                     IPv4 模式 (默认)
  -6                     IPv6 模式
  --bind <host>          绑定本地地址 (多网卡场景)

Server Options:
  -p, --port <port>      监听端口号 (默认: 5201)
  -1                     单次模式：处理一个客户端后退出
  --idle-timeout <sec>   客户端无数据超时时间 (默认: 5 秒)

Common Options:
  -J, --json             以 JSON 格式输出
  --logfile <file>       将输出同时写入文件
  --forceflush           实时刷新日志 (无缓冲)
  -V, --version          显示版本信息
  -h, --help             显示帮助信息
```

### 比特率后缀

| 后缀 | 含义 | 示例 |
|------|------|------|
| `b` | bps | `-b 1000000b` = 1 Mbps |
| `k` / `K` | Kbps | `-b 1000k` = 1 Mbps |
| `m` / `M` | Mbps | `-b 10m` = 10 Mbps |
| `g` / `G` | Gbps | `-b 1g` = 1 Gbps |
| 无后缀 | bps | `-b 1000000` = 1 Mbps |

### 使用示例

```bash
# === 基础测试 ===
# 服务端（默认端口 5201）
nb.exe -s

# 客户端（10 Mbps, 30 秒）
nb.exe -c 192.168.1.100 -b 10m -t 30

# === JSON 输出 ===
nb.exe -c 192.168.1.100 -J

# === IPv6 ===
nb.exe -s -6
nb.exe -c ::1 -6

# === 单次模式 ===
nb.exe -s -1        # 服务端处理完一个客户端后退出

# === 日志输出 ===
nb.exe -c 192.168.1.100 -J --logfile result.json

# === 自定义报文大小 ===
nb.exe -c 192.168.1.100 -l 1000    # 1000 字节报文
```

---

## 输出格式

### 终端输出

```
[ ID] Interval        Transfer      Bitrate       Jitter   Lost/Total   Loss%  OoO
-------------------------------------------------------------------------------
[  1] 0.00-1.00  sec    1.25 MBytes   10.5 Mbps   0.123ms   23/ 1012    2.27%   0
[  1] 1.00-2.00  sec    1.24 MBytes   10.4 Mbps   0.098ms   18/ 1005    1.79%   1
-------------------------------------------------------------------------------
[  1] 0.00-10.00 sec   12.5 MBytes   10.5 Mbps   0.112ms    0/    0     --%   2  sender
[  1] 0.00-10.00 sec   12.3 MBytes   10.4 Mbps   0.112ms  210/10180    2.06%   2  receiver
-------------------------------------------------------------------------------
Lost: 210/10180 (2.06%)
```

客户端显示双行（sender / receiver），类似 iperf3。末行 "Lost: X/Y (Z%)" 为服务端权威丢包率。未收到服务端 Result 时显示 `--/--`。行尾 `sender`/`receiver` 标签区分角色。

### JSON 输出

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

`end` 段使用扁平结构（无嵌套 `client_stats`/`server_stats`）。服务端数据到达时作为权威来源填入 `end`，否则填入客户端本地数据。区间段（`intervals`）的丢包数据仅来自发送端，故 `lost_packets`/`total_packets` 无意义。

---

## 状态机

### 服务端

```
                  ┌───────────┐
                  │ Ctrl+C    │
                  │ (任何状态) │
                  └─────┬─────┘
                        │
    ┌───────────┐       ▼
    │ LISTENING │ ── Start ──→ RECEIVING ── Finish ──→ REPORTING
    │           │                                          │
    │ 空超时    │←──────────── 超时 ──────────┐             │
    │ 继续监听  │                              │             │
    └───────────┘                         ┌───┴───┐         │
                                          │ IDLE  │<────────┘
                                          │ -1→退出          │
                                          │ 非-1→LISTENING  │
                                          └───────┘
```

### 客户端

```
RESOLVING ──→ SENDING ──→ WAITING ──→ REPORTING
                   │          │
                   │ Ctrl+C   │ 超时
                   ▼          ▼
              REPORTING (部分结果)
```

---

## 关键模块详解

### UdpSocket

RAII 封装的 Winsock UDP socket，核心 API：

| 方法 | 说明 |
|------|------|
| `create_client(ipv6)` | 工厂方法，创建未绑定的客户端 socket |
| `bind(addr, port)` | 绑定到本地地址 |
| `connect(addr)` | UDP 虚拟连接（用于 ICMP 错误检测） |
| `send_to(data, len, dest)` | 发送 UDP 数据报 |
| `recv_from(buf, size, &src)` | 接收 UDP 数据报（返回 -1 超时） |
| `set_recv_timeout(ms)` | 设置接收超时 |
| `set_buffers(rcv_kb, snd_kb)` | 调整 socket 缓冲区大小 |
| `check_error()` | 检查异步错误（ICMP 不可达等） |

### Pacer

混合精度带宽控制器：

```
Pacer(bitrate_bps, packet_size):
  interval_ns = (packet_size * 8 / bitrate) * 1e9

  if interval_ns < 2_000_000 (2ms):
    burst_size = ceil(2ms / interval_ns)
  else:
    burst_size = 1
```

2ms 阈值对齐 Windows sleep_for 的精度下限。≤2ms 的间隔走纯 spin-wait + burst 模式，>2ms 的间隔先 sleep_for 再 spin 补偿剩余微秒。

### StatsCollector

单线程使用的统计收集器（仅服务端调用，无并发访问）：

| 方法 | 说明 |
|------|------|
| `start_test(duration)` | 重置所有计数器 |
| `record_packet(hdr, recv_time)` | 记录一个接收到的数据包 |
| `finalize()` | 生成最终统计摘要（含丢包率） |
| `set_sender_packets(count)` | 设置发送端发包总数（从 Finish 消息解析） |


### ControlProtocol

| 方法 | 说明 |
|------|------|
| `send_start(sock, dest, duration)` | 发送 Start 控制包（8B payload: duration_sec + reserved） |
| `send_finish(sock, dest, total_packets)` | 发送 Finish 控制包（8B payload: 发送端总发包数 `uint64_t`） |
| `send_result(sock, dest, summary)` | 发送 Result（JSON payload ≤1024B，最多重试 3 次，200ms 超时等 ACK） |
| `send_ack(sock, dest)` | 发送 ACK（1 字节 0x00） |
| `parse_start_duration(data, len)` | 解析 Start payload 中的 duration_sec |
| `parse_result(data, len)` | 解析 Result JSON → StatsSummary（catch 异常） |
| `parse_finish_total_packets(data, len)` | 解析 Finish payload 中的总发包数（8 字节 `uint64_t`） |

---

## Windows 平台注意事项

| 问题 | 处理方式 |
|------|---------|
| **WSAStartup/WSACleanup** | `WsaContext` RAII 类在 main() 中管理生命周期 |
| **SO_RCVBUF 默认 8KB** | 设为 2MB，Windows 可能自动翻倍 |
| **SO_SNDBUF 默认 8KB** | 设为 512KB |
| **sleep_for() 精度 ~1ms** | 混合 spin-wait + _mm_pause() 达 μs 级 |
| **sleep_for 精度受限** | 默认定时器~15.6ms — 采用 2ms 阈值：>2ms 用 sleep_for + spin 补偿，≤2ms 纯 spin-wait |
| **Ctrl+C** | `SetConsoleCtrlHandler` 注册回调，设置 `g_shutdown = true` |
| **防火墙** | 首次启动 Server 弹窗提示 |
| **ICMP 检测** | `connect()` 虚拟连接 + `getsockopt(SO_ERROR)` |
| **静态链接 CRT** | 无需安装 vcredist |

---

## 调试技巧

**编译后快速测试：**
```powershell
# 终端 1: 服务端
nb.exe -s -p 5201

# 终端 2: 客户端
nb.exe -c 127.0.0.1 -b 10m -t 5
```

**查看依赖：**
```powershell
dumpbin /dependents nb.exe
# 预期只显示: KERNEL32.dll, WS2_32.dll, MSVCP140.dll(若动态链接)
```

**抓包分析：** 用 Wireshark 过滤 `udp.port == 5201` 查看协议头。

---

## 已知限制

- UDP 协议本身不保证可靠传输，高丢包环境会影响测量准确性
- 服务端为单线程，10Gbps 以上可能 CPU 饱和（~80% 单核）
- 抖动计算受时钟漂移影响（~25ppm，30s 测试 ~0.75ms 误差）
- 单流模式，不支持并行流
- 不支持 TCP 模式（v1.0 仅 UDP）
- Windows sleep_for 无 sub-2ms 精度保证 — 高码率下 spin-wait 占满一核 CPU
- `high_resolution_clock::time_since_epoch()` 在 MSVC 上返回开机时长而非 Unix 时间戳（不影响 RFC 3550 抖动计算，仅影响 JSON 中的绝对时间可读性）

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0.0 | 2026-07-26 | 初始版本 |
