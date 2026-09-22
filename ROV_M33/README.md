# ROV_M33 —— M33 实时控制固件与 A35 配套软件

STM32MP257（ATK-DLMP257B）**Cortex-M33** 侧的 ROV 实时控制固件，以及运行于 **Cortex-A35 / Linux** 的配套 RovControl 库与上电自检工具。

M33 固件为**裸机（bare-metal）**设计：OpenAMP/RPMsg 命令服务、PCA9685 十六路执行输出、MPU6500/DYP-L08 采集、姿态估计与双轴 PID 稳定闭环，全部运行在确定性主循环中——**刻意不引入 RTOS 内核**，实时性不受 Linux 侧任何负载影响，控制面与业务面彻底解耦（岸端断线时姿态稳定依旧工作）。

## 系统定位

```text
Cortex-A35 (Linux)                          Cortex-M33 (本工程, 裸机)
Gateway_A35 ──RovControl──▶ /dev/ttyRPMSG0 ──OpenAMP/RPMsg──▶ CommandService
                                                                    │
     PCA9685 (I²C4) ◀── ActuatorService ◀── Servo/PropellerService ◀┘
     10 舵机 CH0-9 · 4 垂直 CH10-13 · 2 水平 CH14-15
     MPU6500 (I²C8, 100Hz) ─▶ AttitudeEstimator ─▶ StabilityControl(PID/Mixer, 50Hz)
     DYP-L08 (UART4) ─▶ SensorService
```

## M33 固件架构

启动流：`Reset_Handler` → NonSecure `SystemInit`（使能 CP10/CP11 FPU 访问，DSB/ISB）→ HAL/IPCC 平台同步 → OpenAMP/RPMsg 初始化与 VIRT_UART 端点注册 → DMA/I2C4、I2C8 初始化 → ActuatorService / PropellerService（安全中位）→ SensorService → AttitudeEstimator / StabilityControl → 主循环。

主循环（无 RTOS，轮询分频）：

```text
while (1)
  ├─ OPENAMP_check_for_message()          # RPMsg 收包
  ├─ 100 Hz tick: MPU6500 采集 + 姿态估计
  ├─ OPENAMP_check_for_message()
  ├─ 50 Hz tick: StabilityControl（双轴 PID）+ 混控输出
  ├─ OPENAMP_check_for_message()
  └─ CommandService: 信封解析 / SEQ 保全 / 分发 / 应答
```

- **执行链**：Servo CH0~CH9（0~180°）；Propeller CH10~CH15（命令值 −100~+100%，按 `PWM_us = 1500 + 5 × command` 映射 1000~2000µs）；组命令/模式切换/`all stop` 按语义提交通道组；上电与异常时推进器强制安全中位。
- **安全语义**：全局/垂直/水平 stop 锁存与 move 允许、stabilization / synchronization 模式、命令可选四位十进制 SEQ（`<SEQ> <payload>` 或裸 payload）。
- **初始化失败策略**：OpenAMP 为 fail-stop 基础设施；I2C8/MPU6500、SensorService、姿态与稳定模块失败仅记日志并表现为 not-ready，不剥夺 RPMsg 诊断通道、不使能 PID。
- 源码模块（`STM32CubeIDE/CM33/NonSecure/Application/User/`）：`Core`、`OPENAMP`、`Command`、`Servo`、`Propeller`、`Actuator`、`PCA9685`、`MPU6500`、`DYP`、`Sensor`、`Attitude`、`Control`、`LU9685`。

## 命令集（A35↔M33，v1.0）

```text
set servo <id> <angle> | set servo all <angle> | get servo <id> | get servo all
set servo <id> mid | set servo all mid
set propeller vertical base <value> | set propeller vertical <id> <value>
set propeller horizontal base <value> | set propeller horizontal <id> <value>
set propeller all stop
get propeller <id> base | get propeller all base | get propeller <id> real | get propeller all real
horizontal on | horizontal off | synchronization on | synchronization off
sensor mpu
```

完整值域、模式与安全返回见 `project-docs/ROV_A35_M33_Control_Protocol_v1.0.md`。

## A35 侧配套（`A35/rov_control`）

运行路径 `RovControl → RpmsgClient → ITransport → PosixTtyTransport`：独占 reader 线程、pending 请求表、四位 SEQ 分配/超时/迟到响应隔离，API 覆盖 Servo / Propeller base+individual+query、稳定与同步模式、三级 stop/move 锁存、MPU / DYP / 传感器快照 / 姿态 / 稳定遥测。

配套工具：

- `rov_self_test`：A35 上电自检（默认低危只读 + `stop`；`--actuators`/`--thrusters` 需交互确认），2026-08-31 实机验收零 FAIL；
- `rov_api_smoke`：开发者诊断/故障隔离工具（曾实证 CH9 舵机物理动作）。

## 稳定基线（历史锚点）

- 版本 `v0.1.0`，Git 标签 `v0.1.0-hardware-baseline`（内嵌仓库历史现备份于仓库旁 `STM32MP257_ROV_M33_git_history/`）；
- CubeIDE 工程 `STM32CubeIDE/CM33/NonSecure`，构建配置 `CA35TDCID_m33_ns_sign`；
- 该基线已实机验证：Linux `remoteproc0` 启动、`/dev/ttyRPMSG0` 通信、Servo/Propeller/MPU6500/PCA9685 输出链路、统一 CommandService/SEQ 解析。后续开发从该基线演进，不覆盖标签。

## 能力与验证边界（如实声明）

| 层级 | 状态 |
|---|---|
| RPMsg / PCA9685 / 舵机物理动作 / MPU6500 / DYP-L08 / INA226 链路 | **实机验证通过** |
| 裸机横滚/俯仰姿态稳定（AttitudeEstimator + PID + Mixer） | **台架验证通过**（2026-08-30，舵机替代负载） |
| 真实 ESC/推进器与水下闭环、水下 PID 整定 | 待整机集成后验证 |
| A35 侧 RovControl API v1 / rov_self_test | **实机验收通过**（2026-08-31，主机 90/90 测试） |

## 构建、签名与部署

STM32CubeIDE 1.17.0 打开 `STM32CubeIDE/CM33/NonSecure`，以 `CA35TDCID_m33_ns_sign` 配置构建，生成 ELF / stripped ELF / BIN。**编译后签名**：调用 `Utilities/optee_os/scripts/sign_rproc_fw.py`（依赖 pycryptodomex / pyelftools，仓库随附便携环境 `Tools/python`）产出 `ROV_M33_CM33_NonSecure_sign.bin`。

部署流程：先上传临时路径 → 核对 SHA-256 → 复制到 `/lib/firmware/ROV_M33_CM33_NonSecure.elf` → remoteproc 加载。构建、部署、哈希校验与验证记录规则见 `project-docs/DEVELOPMENT.md` 与 `project-docs/DEVELOPMENT_LOG.md`；开机/换载 SOP 见 `project-docs/ROV_RUNTIME_HANDOFF.md`。

## 主机单元测试（`Tests/`）

`attitude_estimator` / `command_service` / `dyp` / `mpu6500` / `pca9685` / `pid_controller` / `propeller_service` / `servo` / `stability_control` / `vertical_mixer`（含 mocks），与 A35 侧 72+18 项测试共同构成 **90/90 主机测试基线**（MSVC `/W4 /WX` 零警告）。

## 目录

```text
ROV_M33/
├── A35/rov_control/        # A35 侧 RovControl 库 + rov_self_test / rov_api_smoke
├── CM33/NonSecure/         # M33 共享源码树（Core / Drivers / OPENAMP / Services）
├── Middlewares/Third_Party/OpenAMP/   # OpenAMP 中间件
├── STM32CubeIDE/CM33/NonSecure/       # CubeIDE 工程（Application/User 各模块 + 构建配置）
├── Tests/                  # M33 模块主机单元测试
├── Tools/python/           # 固件签名脚本便携 Python 环境（vendored，不计入语言统计）
└── project-docs/           # 架构/决策/开发日志/协议/交接与故障排查文档
```
