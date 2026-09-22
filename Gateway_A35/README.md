# rov_gateway（Gateway_A35）—— A35 端协议网关与权威状态服务

运行于 STM32MP257 ATK-DLMP257B **Cortex-A35 / OpenSTLinux** 的 ROV 网关服务，居于系统三层"双盲"架构的正中：

```text
Windows Salacia_Terminal  ←TCP :7000→  rov_gateway(A35)  ←RovControl/RPMsg→  M33 固件
        （只懂业务协议）              唯一协议转换者                 （只懂 ASCII 协议）
```

Windows 与 M33 互相不可见：A35 负责 Windows 业务请求 → RovControl/M33 的全部转换、执行器通道映射、安全裁决，并作为**权威状态**与传感器汇聚节点（DHT11 / INA226 本地采集 + M33 上报数据，合成 100Hz 遥测汇总流经 UDP 发往终端）。

## 当前状态：阶段 0~6 全部完成（2026-09-03 实机证据，2026-09-05 入库）

- 阶段 0~2：只读审查 / wire 协议层（CRC16-CCITT-FALSE、流式分帧、41 项功能注册表、NaN/Inf 消毒）/ 核心调度（9 位权威状态机、优先级命令队列、Safe 联动、Stop-Move 三级锁存、Estop-Emergency、推进器翻译矩阵、BaseValueVH 原子性、断线不重放），WinSock/POSIX 可移植单客户端 TCP 服务端；
- 阶段 3：传感器融合（Dht11Reader misc/IIO/auto、Ina226Reader hwmon 发现+重扫、M33SensorReader MPU 换算与 DYP 窗口判定、SensorService 独立线程 + TTL + 100Hz 汇总流）；
- 阶段 4：实机 RPMsg 联调（板端 ctest 13/13、探针全 PASS、**Estop 往返 5.1~7.2ms**）；
- 阶段 5：OpenSTLinux 部署（**开机自启动已验证**、INI 配置、systemd、运维手册）；
- 阶段 6：Windows 实机对接（**48 项 LAN 一致性 ALL PASS** + 真实终端连接，100Hz 汇总流板上抓包 201 帧/2s 确认送达）。

实机证据摘要（详见 `DECISIONS.md` R-01~R-05）：启动对齐 mask=0x0066 正确；DHT11 实读 4930=49%RH/30℃；DYP 65533 正确判无效；SIGTERM 干净退出且 M33 全局停止锁存保持；Windows 主机直连 192.168.1.120:7000 字节级验证通过。

已知问题：T-01 RPMsg 长跑退化（M33 侧根因未闭环）。已部署自动恢复：MPU 数据链路看门狗（`[gateway] mpu_watchdog_timeout_ms`，默认 60s，0=禁用）在 M33 楔死时自动退出网关并由 systemd 重启链恢复（`rov_m33_gate.sh` 重启 M33 + 自检落 stop 锁存）；MPU/DYP 轮询失败有限频 journal 日志。2026-09-04 新增 INA226/DHT11 开机自准备单元（rov-ina226/rov-dht11）。分析见 `TROUBLESHOOTING.md`。

设计依据：`PHASE0_REVIEW.md`（只读审查与全量映射）、`DECISIONS.md`（用户批复 U-00~U-05、设计决策 D-01~D-31、实机证据 R-01~R-08）、`Salacia_Terminal/docs/WINDOWS_A35_INTERFACE.md`（TCP 协议最终权威）、`ROV_M33/project-docs/ROV_A35_M33_Control_Protocol_v1.0.md`（A35↔M33 业务语义）。

## 架构

```text
┌─ net/ ─────────────────────────────────────────────────────┐
│ 可移植 socket 层（WinSock/POSIX）+ 单客户端 TCP 服务端 :7000 │
└──────┬─────────────────────────────────────────────────────┘
       ▼
┌─ wire/ ────────────────────────────────────────────────────┐
│ CRC16-CCITT-FALSE · 帧编解码 · 流式分帧 · 41 项功能注册表    │
│ 类型化载荷解析/构造（NaN/Inf 消毒）                          │
└──────┬─────────────────────────────────────────────────────┘
       ▼
┌─ core/ ────────────────────────────────────────────────────┐
│ 9 位权威状态机 · 优先级命令队列（Estop>Emergency>Stop/Move>普通）│
│ Safe 联动 · Stop-Move 三级锁存 · 推进器翻译矩阵 · 断线不重放  │
│ IRovControl 接缝 → rov_control_adapter（板端纯转发真机）      │
└──┬───────────────────────────────┬─────────────────────────┘
   ▼ vendor/rov_control            ▼
   RovControl API → /dev/ttyRPMSG0 │ 1Hz 状态兜底 · 心跳监督 · SIGTERM 优雅退出
   （OpenAMP/RPMsg → M33）         │ + best-effort stop
┌─ sensors/ + util/ ───────────────┴─────────────────────────┐
│ Dht11Reader · Ina226Reader · M33SensorReader(MPU/DYP)       │
│ SensorService：独立线程 · TTL 有效性 · 100Hz UDP 汇总流 → 终端 │
└────────────────────────────────────────────────────────────┘
```

## 板端操作（当前部署）

**开机自启动已启用**（`rov_gateway.service`，enable 状态）。上电后自动完成：vendor 固件服务 → preflight 换载 ROV 固件并启动 M33 → 等 RPMsg → self-test → 网关监听 0.0.0.0:7000。日志：`journalctl -u rov_gateway -f`。

```bash
# 服务管理
systemctl status rov_gateway          # 状态（active = 正常）
systemctl restart rov_gateway         # 重启网关（不重启 M33，锁存保持）
systemctl stop rov_gateway            # 停止（M33 stop 锁存保持）
journalctl -u rov_gateway -f          # 实时日志

# 验证
/home/root/gateway/build/gateway_probe 7000 20

# 配置（/etc/rov_gateway.ini，示例见仓库 config/rov_gateway.ini）
vi /etc/rov_gateway.ini && systemctl restart rov_gateway

# 视频推流（默认未启用；需摄像头接好时手动开启）
systemctl enable --now rov_camstream  # 目标地址见 [camstream] 节

# 源码更新部署（主机侧修改后经 scripts/board_ssh.py 上传，然后）
cmake --build /home/root/gateway/build -j2 && ctest --test-dir /home/root/gateway/build
systemctl restart rov_gateway
```

注意：网关运行期间独占 `/dev/ttyRPMSG0`——不得并发 `cat`/`echo`/`rov_self_test`/`rov_api_smoke`/第二个网关实例。

## 构建

板端（OpenSTLinux，正式路径）：

```bash
cmake -S /home/root/gateway -B /home/root/gateway/build -DCMAKE_BUILD_TYPE=Release
cmake --build /home/root/gateway/build -j"$(nproc)"
ctest --test-dir /home/root/gateway/build --output-on-failure
```

主机（无 CMake 时的等价构建，Windows MinGW / Linux）：

```bash
bash scripts/build_host.sh      # 产出 build_host/test_* 与 build_host/rov_gateway
bash scripts/run_tests_host.sh  # 运行全部测试（带超时保护）+ --check 自检
```

主机构建脚本与 CMakeLists 编译同一组源文件；新增源文件时两处同步。Windows 主机链接 `-lws2_32`，POSIX 使用 `MSG_NOSIGNAL` 并忽略 SIGPIPE。

## 测试

- 主机 **14 个测试套件**全过（协议层/核心调度/传感器/集成）；
- 板端同源 ctest **13/13**；
- 阶段 6 局域网行为一致性 **48 项 ALL PASS**（连接、状态、指令、重连、断线回退等）。

## 目录

```text
Gateway_A35/
├── PHASE0_REVIEW.md          # 阶段0只读审查（全量映射表/歧义/文件清单）
├── DECISIONS.md              # 用户批复（U-00~U-05）、设计决策（D-01~D-31）与实机证据（R-01~R-08）
├── TROUBLESHOOTING.md        # 已知问题（T-01 RPMsg 长跑退化）与排查
├── Drivers.md                # DHT11/INA226 驱动要求（用户提供的规格）
├── CMakeLists.txt            # 板端链接 vendor+adapter+完整 main；主机为骨架
├── config/rov_gateway.ini    # 配置样例
├── deploy/                   # systemd 单元与板端脚本（网关/换载/preflight/camstream）
├── src/
│   ├── wire/                 # 协议层
│   ├── core/                 # 状态机/调度/优先队列/M33 接缝+真机适配
│   ├── net/                  # 可移植 socket + 单客户端 TCP 服务端
│   ├── sensors/              # DHT11/INA226/M33 采集 + SensorService
│   ├── util/                 # 分级日志（限频）
│   ├── main.cpp              # 主机骨架（--check 自检）
│   └── main_gateway.cpp      # 板端完整 main
├── tests/                    # 主机测试套件（板端同源 ctest）
├── tools/gateway_probe.cpp   # 板端验证客户端
├── scripts/                  # 主机构建/测试 + board_ssh.py 板端运维
└── vendor/rov_control/       # RovControl API v1 库源码（原样复制，板端链接）
```

## 运行边界（红线）

- 网关是 A35 上唯一持有 `/dev/ttyRPMSG0` 的进程；运行期间禁止 `cat`/`echo`/第二个 RovControl 进程/并行的 `rov_self_test`。
- 不生成 M33 ASCII 命令、不直接读写 `/dev/ttyRPMSG0`，一切经 RovControl。
- 不修改 Windows 终端、M33 固件、wire 协议、OpenAMP/remoteproc 与设备树。
