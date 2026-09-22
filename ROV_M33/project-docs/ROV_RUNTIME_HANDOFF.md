# ROV 板端启动与 RovControl API 交接手册

本文面向未参与前期调试的队友，说明 STM32MP257 ROV 每次上电后的正式启动流程、首次部署或代码更新后的构建流程、`rov_self_test` 使用方法、Qt/业务程序接管顺序以及常见故障处理。

本文是运行 SOP，不是开发日志。M33 wire protocol 的详细定义仍以 `ROV_A35_M33_Control_Protocol_v1.0.md` 为准。

## 1. 当前稳定基线

```text
当前 HEAD / rov_self_test:
19f6f9f7e2852edfdd8cc95d4ced2744a41b6782
a35: add validated ROV self-test

A35 RovControl API v1:
f95b0eca57be15ff74732983562905fe3ba74f06
a35: complete validated RovControl API v1

Host tests at this baseline: 90/90 PASS
Default rov_self_test real-board acceptance: PASS
```

以后更新代码时，在正式部署前执行：

```bash
git rev-parse HEAD
```

必须记录实际部署的 Git SHA。本文中的测试数量是当前基线事实，不是永久不变的协议要求。

## 2. 系统组成与职责

```text
Qt / business application
  -> RovControl
     -> RpmsgClient
        -> PosixTtyTransport
           -> /dev/ttyRPMSG0
              -> OpenAMP/RPMsg
                 -> M33 CommandService
                    -> Servo / Propeller / Sensor / Control services
```

- M33：裸机实时控制，管理 PCA9685、Servo/Propeller、MPU6500、DYP、姿态与稳定控制。
- A35/Linux：运行 `rov_self_test`、`rov_api_smoke` 和未来 Qt/业务程序。
- `RovControl`：A35 业务程序唯一应使用的硬件控制 API。
- `rov_self_test`：正式通电/换板/刷机后的健康检查。
- `rov_api_smoke`：开发者故障定位工具，不是日常启动必需步骤。

## 3. 固定板端路径

板端运行命令统一使用绝对路径，以避免当前工作目录不同造成失败，也便于 Qt `QProcess`、systemd 或脚本直接复用。

| 用途 | 正式路径 |
|---|---|
| M33 启动脚本 | `/home/root/ROV_M33/lib/fw_cortex_m33.sh` |
| M33 staging ELF | `/home/root/ROV_M33/lib/firmware/ROV_M33_CM33_NonSecure.elf` |
| Linux firmware ELF | `/lib/firmware/ROV_M33_CM33_NonSecure.elf` |
| remoteproc state | `/sys/class/remoteproc/remoteproc0/state` |
| remoteproc firmware | `/sys/class/remoteproc/remoteproc0/firmware` |
| RPMsg endpoint | `/dev/ttyRPMSG0` |
| RovControl 根目录 | `/home/root/rov_control` |
| 正式 self-test | `/home/root/rov_control/build/rov_self_test` |
| 开发 smoke tool | `/home/root/rov_control/build/rov_api_smoke` |

源码位置在仓库内使用相对路径，例如 `A35/rov_control/tools/rov_self_test.cpp`。不要把开发机个人绝对路径写入脚本或项目配置。

## 4. 正常每次开机 SOP

> **重要：正常开机不需要重新执行 CMake、build 或 CTest。** 只要 `/home/root/rov_control/build/rov_self_test` 已存在并对应当前正式代码，就直接运行现有二进制。

### Step 1：启动 M33

目的：部署/选择当前 M33 ELF，并通过 remoteproc 启动 M33。

执行：

```bash
/home/root/ROV_M33/lib/fw_cortex_m33.sh start
```

随后确认状态：

```bash
cat /sys/class/remoteproc/remoteproc0/state
cat /sys/class/remoteproc/remoteproc0/firmware
```

成功标志：

```text
state: running
firmware: 包含 ROV_M33_CM33_NonSecure.elf
```

失败时先检查：

```bash
cat /sys/class/remoteproc/remoteproc0/state
dmesg | grep -i remoteproc
```

如果脚本报告失败但 state 已是 `running`，先确认当前 firmware 是否正确，不要未经判断就反复 stop/start。

### Step 2：确认 RPMsg endpoint

目的：确认 M33 已创建 A35 API 所需的通信端点。

执行：

```bash
ls -l /dev/ttyRPMSG0
```

成功标志：`/dev/ttyRPMSG0` 存在。

如果不存在，不要继续启动 self-test、RovControl 或 Qt。检查：

```bash
cat /sys/class/remoteproc/remoteproc0/state
dmesg | grep -i remoteproc
dmesg | grep -i rpmsg
```

`RovControl::open()` 和 `rov_self_test` 都假定 M33 与 RPMsg 已经准备好。

### Step 3：运行正式通电自检

目的：通过现有 RovControl API 检查 RPMsg、M33 服务、传感器、状态解析和安全停止。

执行：

```bash
/home/root/rov_control/build/rov_self_test
echo $?
```

当前默认检查包括：

- RPMsg open
- CH0～CH9 Servo 软件状态
- CH10～CH15 Propeller output/base 软件状态
- MPU6500 原始数据
- DYP UART 通信
- Sensor snapshot
- Attitude
- Stabilization telemetry
- Global safe stop

成功判断以以下两项为准：

```text
FAIL=0
exit code=0
```

最终文本可能是 `RESULT: PASS`，也可能因为未运行可选 actuator/thruster 检查而显示 `RESULT: PASS WITH WARNINGS`。当前默认板端验收结果为 FAIL=0、WARN=0、optional SKIP、exit code 0。

Exit code：

| Code | 含义 |
|---|---|
| `0` | PASS 或 PASS WITH WARNINGS，可以进入下一状态 |
| `1` | self-test 失败，不应进入运动状态 |
| `2` | CLI 参数错误 |

### Step 4：确认 self-test 后的安全状态

> **WARNING — self-test 成功结束后，global stop 仍然保持 latched。**

这表示系统健康且处于 READY/SAFE，但推进器仍禁止运动。这不是错误。

正式开始运动前，拥有控制权的 Qt/业务程序必须在完成自身安全检查并得到用户明确授权后调用：

```cpp
rov.move();
```

`move()` 只解除 global stop latch，不会自动让推进器转动。self-test 不会自动调用 `move()`。

## 5. 首次部署或 RovControl 更新后的 Build SOP

以下情况需要重新 build：

- 首次部署，`/home/root/rov_control/build` 不存在。
- `A35/rov_control` 源码更新。
- `CMakeLists.txt` 更新。
- build 目录损坏或工具链变化。
- 需要执行正式 clean-build 验收。

### 5.1 正式 clean build

```bash
rm -rf /home/root/rov_control/build
cmake -S /home/root/rov_control -B /home/root/rov_control/build -DCMAKE_BUILD_TYPE=Release
cmake --build /home/root/rov_control/build -j"$(nproc)"
ctest --test-dir /home/root/rov_control/build --output-on-failure
```

说明：

- `rm -rf /home/root/rov_control/build` 只删除明确的 build 输出目录，用于强制全量重编译。
- Clean build 耗时较长，不应在每次开机时执行。
- 成功标志是 build 无 error、CTest 全部 PASS。
- 当前稳定基线的测试输出为 90/90 PASS；后续新增测试后数量可能变化。

### 5.2 小范围代码更新后的增量 build

```bash
cmake --build /home/root/rov_control/build -j"$(nproc)"
ctest --test-dir /home/root/rov_control/build --output-on-failure
```

### 5.3 正常开机时不需要做什么

正常每次上电不要执行：

```bash
rm -rf /home/root/rov_control/build
cmake -S /home/root/rov_control -B /home/root/rov_control/build
cmake --build /home/root/rov_control/build
ctest --test-dir /home/root/rov_control/build
```

CMake/build 是把源码编译为二进制；self-test 是运行已经编译好的二进制。两者不是同一件事。

## 6. Self-test 可选模式

### 6.1 默认安全模式

```bash
/home/root/rov_control/build/rov_self_test
```

默认不发送 Servo setter 或非零推进器命令，并最终保持 global stop。

### 6.2 Servo actuator 模式

```bash
/home/root/rov_control/build/rov_self_test --actuators
```

- 必须在交互终端输入完整大写 `YES`。
- 查询 CH9 当前软件角度，执行 ±5° 小偏移，查询软件状态，然后立即恢复原角度。
- 两次 setter 之间没有显式保持 delay，动作可能很小且短暂。
- PASS 证明 software command/query/restore path；`getServo()` 不是机械位置反馈。
- 物理舵机动作必须由人观察。不要写成“self-test 自动验证舵机到位”。

既有 `/home/root/rov_control/build/rov_api_smoke servo` 曾产生明显 CH9 实际动作，这是独立的物理链路证据，不属于 self-test 自动反馈。

### 6.3 Thruster safety/state 模式

```bash
/home/root/rov_control/build/rov_self_test --thrusters
```

- 必须在交互终端输入完整确认词 `I_UNDERSTAND`。
- 第一版不发送非零推进器命令。
- 它建立 global stop，发送 vertical base 零命令并要求得到 `err safety`，再确认 CH10～CH15 软件 output 均为零。
- 这是 safety/state logic 检查，不是推进器物理动作测试。
- 不能据此声称真实推进器、推力、水下动力学或闭环控制已验证。

执行全部可选模式：

```bash
/home/root/rov_control/build/rov_self_test --all
```

## 7. Qt / 业务程序接管流程

推荐状态机：

```text
IDLE
  -> STARTING_M33
  -> WAITING_RPMSG
  -> SELF_TESTING
  -> READY_SAFE
  -> user authorizes motion
  -> ACTIVE

任何步骤失败 -> ERROR
```

推荐初始化顺序：

1. 用 `QProcess` 启动 `/home/root/ROV_M33/lib/fw_cortex_m33.sh`，参数为 `start`。
2. 等待进程完成并检查 exit code；不要只检查“进程启动成功”。
3. 读取 `/sys/class/remoteproc/remoteproc0/state`，确认内容为 `running`。
4. 使用 `QFile::exists("/dev/ttyRPMSG0")` 或带超时的轮询等待 endpoint。
5. 用另一个 `QProcess` 运行 `/home/root/rov_control/build/rov_self_test`。
6. 等待 self-test **完全退出**，检查 exit code 为 0。
7. self-test 退出后，Qt 才创建并 `open()` 自己的 `RovControl`。
8. UI 显示 `READY / SAFE`；此时 global stop 仍保持。
9. 用户明确授权运动后，Qt 调用 `move()`。

QProcess 启动示意：

```cpp
QProcess *m33Start = new QProcess(this);
m33Start->start("/home/root/ROV_M33/lib/fw_cortex_m33.sh",
                QStringList{"start"});

// 检查 finished、exitStatus 和 exitCode 后，再等待 RPMsg。
if (QFile::exists("/dev/ttyRPMSG0")) {
    QProcess *selfTest = new QProcess(this);
    selfTest->start("/home/root/rov_control/build/rov_self_test",
                    QStringList{});
}
```

不要把启动脚本、endpoint 检查和 self-test 粗暴拼成一个 `shell -c "... && ..."` 后只看最终结果。Qt 应逐步检查每个状态，并设置合理超时。

> `rov_self_test` 必须先退出，Qt 才能打开 RovControl。两者不能同时持有 `/dev/ttyRPMSG0`。

## 8. RovControl API 使用边界

正常 Qt/业务代码只使用 `RovControl`。不要直接操作：

- `/dev/ttyRPMSG0`
- ASCII command 或四位 SEQ
- PCA9685/I2C/UART
- M33 HAL 或 M33 service 内部函数

最小示意：

```cpp
#include "rov/rov_control.hpp"

rov::RovControl rov;

const auto opened = rov.open();
if (!opened) {
    // 使用 opened.failure.code/origin/detail 报错。
    return;
}

const auto sensors = rov.getSensorSnapshot();
const auto attitude = rov.getAttitude();

// self-test 后仍处于 global stop。只有用户明确授权运动后才解除。
const auto moved = rov.move();
if (!moved) {
    return;
}

// 示例值 0 不要求推进器产生运动。
const auto vertical = rov.setVerticalBase(0);
const auto servo = rov.setServo(9U, 90U);
```

当前 public API 以 `A35/rov_control/include/rov/rov_control.hpp` 为准。完整公开签名如下；默认 device 为 `/dev/ttyRPMSG0`：

| 类别 | 当前签名 |
|---|---|
| 构造 | `explicit RovControl(std::string device = "/dev/ttyRPMSG0")` |
| 生命周期 | `RovResult<void> open()`；`void close() noexcept`；`bool isOpen() const noexcept` |
| Servo set | `RovResult<void> setServo(std::uint8_t id, std::uint8_t angle)`；`RovResult<void> setAllServos(std::uint8_t angle)`；`RovResult<void> centerServo(std::uint8_t id)`；`RovResult<void> centerAllServos()` |
| Servo query | `RovResult<std::uint8_t> getServo(std::uint8_t id)`；`RovResult<std::array<std::uint8_t, 10>> getAllServos()` |
| Propeller set | `RovResult<void> setVerticalBase(std::int16_t value)`；`RovResult<void> setVerticalPropeller(std::uint8_t id, std::int16_t value)`；`RovResult<void> setHorizontalBase(std::int16_t value)`；`RovResult<void> setHorizontalPropeller(std::uint8_t id, std::int16_t value)` |
| Propeller query | `RovResult<std::int16_t> getPropellerBase(std::uint8_t id)`；`RovResult<std::int16_t> getPropellerOutput(std::uint8_t id)`；`RovResult<std::array<std::int16_t, 6>> getAllPropellerBases()`；`RovResult<std::array<std::int16_t, 6>> getAllPropellerOutputs()` |
| Modes | `RovResult<void> enableStabilization()`；`RovResult<void> disableStabilization()`；`RovResult<void> enableHorizontalSynchronization()`；`RovResult<void> disableHorizontalSynchronization()` |
| Sensors/control | `RovResult<DypReading> readDyp()`；`RovResult<MpuRaw> readMpu()`；`RovResult<SensorSnapshot> getSensorSnapshot()`；`RovResult<Attitude> getAttitude()`；`RovResult<StabilizationStatus> getStabilization()` |
| Safety latches | `RovResult<void> stop()`；`RovResult<void> move()`；`RovResult<void> stopVertical()`；`RovResult<void> moveVertical()`；`RovResult<void> stopHorizontal()`；`RovResult<void> moveHorizontal()` |

边界说明：

- Servo id：0～9；angle：0～180。
- Vertical Propeller id：10～13；Horizontal Propeller id：14～15；command：-100～100。
- `getServo()` 返回 M33 已成功提交的软件目标角度，不是舵机机械位置反馈。
- Propeller `real/output` 是 M33 软件记录 command，不是 PCA9685 寄存器读回或 RPM 反馈。
- 某些 mode/latch 状态下 setter 或 base query 合法返回 `err safety`。
- `RovResult` 失败时应检查 `failure.code`、`failure.origin` 和 `failure.detail`，不要只显示“failed”。

## 9. RPMsg 独占规则

> **WARNING — `/dev/ttyRPMSG0` 使用 single-reader / single-owner 模型。**

以下任一程序正在使用 endpoint 时，其他程序不得并发读写：

- `RovControl`
- `rov_self_test`
- `rov_api_smoke`
- Qt/正式业务程序

特别禁止与上述程序并发执行：

```bash
cat /dev/ttyRPMSG0
echo "..." > /dev/ttyRPMSG0
```

历史上已经真实发生过第二个 `cat` reader 抢走 response，导致 RovControl timeout。RPMsg tty response 不是广播；另一个 writer 也会破坏 request/SEQ ownership。

正确做法：前一个进程完全退出并释放 endpoint 后，下一个程序才能 `open()`。

## 10. Stop / Move 安全语义

- `stop()`：设置 global Propeller stop latch，并将推进器输出置于停止状态。
- `move()`：只解除 global stop latch，使后续合法运动命令可以生效。
- `move()` 不会自动生成推力，也不会恢复旧输出。
- `stopVertical()` / `moveVertical()` 与 `stopHorizontal()` / `moveHorizontal()` 是独立分组 latch。
- self-test 结束后故意不调用 `move()`。

如果 self-test PASS 但推进器命令返回 `err safety`，首先确认业务程序是否已经在正确的用户授权阶段调用 `move()`。

## 11. DYP 硬件注意事项

UART4 用于 DYP 时，必须移除 J4 上将 CH342F Channel 1 接入 UART4 网络的两个跳帽：

- CH342F TXD1 与 PB6/UART4_RX 必须隔离。
- CH342F RXD1 与 PB7/trigger 网络必须隔离。

跳帽未移除时，CH342F 会对 PB6/PB7 产生电气干扰，真实 DYP 请求可能 timeout。还应检查 DYP 5 V 供电、共地、电平转换方向和 UART4 wiring。

当前观测到的：

```text
distance_mm = 65533
```

只表示收到了 checksum 正确的合法 DYP UART frame。它不能自动解释为真实有效物理距离 65533 mm，也不要在没有厂家文档或后续实验依据时擅自定义为 sentinel。

## 12. rov_api_smoke 的定位

`rov_api_smoke` 是 developer diagnostic / smoke tool，不属于正常每次开机的必做流程。

只读/低风险排障示例：

```bash
/home/root/rov_control/build/rov_api_smoke dyp
/home/root/rov_control/build/rov_api_smoke mpu
/home/root/rov_control/build/rov_api_smoke sensors
/home/root/rov_control/build/rov_api_smoke attitude
/home/root/rov_control/build/rov_api_smoke stabilization
/home/root/rov_control/build/rov_api_smoke servo-get
```

可能产生舵机动作：

```bash
/home/root/rov_control/build/rov_api_smoke servo
```

正式正常启动优先运行 `/home/root/rov_control/build/rov_self_test`。使用 smoke 前必须确认 self-test/Qt/RovControl 已退出，避免争用 RPMsg tty。

## 13. 故障排查速查表

| 现象 | 立即执行/检查 | 判断与下一步 |
|---|---|---|
| `fw_cortex_m33.sh start` 失败 | `cat /sys/class/remoteproc/remoteproc0/state`；`dmesg \| grep -i remoteproc` | 确认是否已 running、firmware 名称是否正确，再决定是否需要重新启动。 |
| `/dev/ttyRPMSG0` 不存在 | `cat /sys/class/remoteproc/remoteproc0/state`；`dmesg \| grep -i remoteproc`；`dmesg \| grep -i rpmsg` | M33/RPMsg 未就绪时不要运行 A35 API。 |
| `rov_self_test` RPMsg open FAIL | `ls -l /dev/ttyRPMSG0`；`ps \| grep -E 'rov_|cat.*ttyRPMSG|Qt'` | 确认 endpoint 存在，并停止其他 owner/reader。 |
| RovControl timeout，但另一个终端看到 response | 查找并停止 `cat /dev/ttyRPMSG0` 或第二个 API/Qt 进程 | 优先按“第二个 reader 抢走 response”处理。 |
| DYP timeout | 检查 J4 两个 UART4/CH342F 跳帽、DYP 5 V、共地、电平转换与 PB6/PB7 wiring | 跳帽未移除是已确认的真实故障原因。 |
| Self-test PASS，但推进器不接受命令 | 确认 self-test 后 global stop；检查上层是否显式调用 `move()` | `move()` 只解除 latch，不自动转动。 |
| 代码更新后 self-test 仍是旧行为 | `git rev-parse HEAD`；检查是否重新 build；`ls -l /home/root/rov_control/build/rov_self_test` | 源码更新后需重建；正常开机则不需要。 |
| Self-test 返回 1 | 查看每个 `[FAIL]` 的 `origin/error/detail` | 不要仅凭最终 RESULT 猜测；逐项处理失败来源。 |

## 14. 当前验证边界

已经验证：

- M33 bare-metal 控制链与 RPMsg。
- A35 RovControl API v1 代表性真实板端调用。
- `rov_self_test` 默认安全模式真实板端 PASS。
- 既有 smoke 中真实 CH9 舵机动作。
- DYP 真实 UART frame、MPU、姿态与稳定 telemetry。

尚未验证：

- `rov_self_test --thrusters` 不是物理推进器测试。
- 真实推进器方向、推力和水下动力学。
- 最终水下闭环与 PID 参数。
- `getServo()` 不提供机械位置反馈。

## 15. 正常每次开机，只需要这些

以下命令可直接复制：

```bash
# 1. 启动 M33
/home/root/ROV_M33/lib/fw_cortex_m33.sh start

# 2. 确认 M33 与 firmware
cat /sys/class/remoteproc/remoteproc0/state
cat /sys/class/remoteproc/remoteproc0/firmware

# 3. 确认 RPMsg
ls -l /dev/ttyRPMSG0

# 4. 正式自检
/home/root/rov_control/build/rov_self_test

# 5. 查看 self-test exit code
echo $?
```

判断：

```text
exit 0 -> 可以由 Qt/RovControl 接管；global stop 仍保持
exit 1 -> 保持安全状态，按 FAIL 项排障
exit 2 -> 修正 CLI 参数
```

即使 exit 0，系统仍处于 global stop latched。Qt/业务程序必须在 self-test 完全退出后才打开 RovControl，并在用户明确授权运动后显式调用 `move()`。
