# 更新日志（CHANGELOG）

记录 Salacia 仓库的重要变更，新条目在前，日期为提交/合并日期。格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)。

## 2026-09-23

### 文档与仓库治理

- 四个子工程 README 全面改版，统一补充工程介绍与架构说明：
  - `Salacia_Terminal`：新增工程定位与多线程架构图（GUI/解码/推理/通信线程模型）；
  - `Gateway_A35`：重构状态章节（合并重复段落）、新增网关架构图与数据流说明；
  - `CamStream`：按当前实现整体重写——OpenCL 色彩转换 + 硬件 H.264 编码管线、参数表、与 systemd 部署的关系；
  - `ROV_M33`：更新能力边界（移除过时的"尚未实现"表述）、新增固件架构与模块图、补充签名构建流程与主机单元测试清单。
- 新增根目录 `CHANGELOG.md`（本文件）。
- 新增 `.gitattributes`：将 `ROV_M33/Tools/python`（固件签名便携 Python 环境）、`Gateway_A35/vendor`、`Salacia_Terminal/src/ui/3rd` 标注为 vendored 代码，修正 GitHub 语言统计（此前仓库语言被第三方 Python 工具环境误判为以 Python 为主）。

## 2026-09-22

### ROV_M33 入库修复

- 修复 ROV_M33 被误提交为子模块链接（gitlink）导致 GitHub 上目录无法打开的问题：解除 gitlink 引用，将 ROV_M33 全部源码（CubeIDE 工程、A35 侧 RovControl 库、project-docs、Tools）以普通文件纳入仓库，共 854 个文件、约 14.8 万行。
- ROV_M33 内嵌独立 `.git`（10 个提交的本地历史）移出仓库备份至仓库旁 `STM32MP257_ROV_M33_git_history/`，未删除任何历史。
- `.gitignore` 新增忽略规则：`ROV_M33/.vs`（VS 缓存）、CubeIDE 编译输出目录 `CA35TDCID_m33_ns_sign/`、`__pycache__/` 与 `*.pyc`。

### 文档

- 根 `README.md` 全面重写：项目亮点、系统总览架构图（陆地层/水面层/水下层）、STM32MP257 与常见 MCU 方案选型对比、执行器与传感器系统、四个软件工程总表、核心技术特性、质量与实测验证、开源组件使用声明、市场定位。

## 2026-09-05

### Gateway_A35（A35 端网关服务）入库

- 新增 A35 端网关程序，阶段 0~6 全部完成（2026-09-03 完成，随本次提交入库）：
  - 协议层：CRC16-CCITT-FALSE、流式分帧、41 项功能注册表、NaN/Inf 消毒；
  - 核心调度：9 位权威状态机、优先级命令队列、Safe 联动、Stop-Move 三级锁存、断线不重放；
  - 传感器融合：DHT11/INA226 驱动发现、M33 数据汇聚、100Hz 汇总流；
  - 实机 RPMsg 联调：板端 ctest 13/13，Estop 往返 5.1~7.2ms；
  - OpenSTLinux 部署：systemd 服务、开机自启动、INI 配置、运维手册；
  - Windows 实机对接：48 项局域网行为一致性 ALL PASS。
- Windows 终端部分问题修复。

## 2026-08-27 ~ 2026-09-02

### Salacia_Terminal（Windows 终端）二轮优化（Phase 12~19）

- 协议层扩展：MoveAll / 分组 Stop-Move / 双 Synchronization / BaseValueVH / StateEventV2（0x0104）权威状态事件，执行器 ID 拓扑常量（舵机 wire 0-9、垂直 CH10-13、水平 CH14-15，UI/wire 三分离）。
- SafetyStateModel 重写（PendingSwitchState×7 事务模型）、ControlViewModel 分组重构、主页/指令页 UI 重构、VideoFrameHub 最新帧零拷贝多视图复用、窗口尺寸修复。
- `docs/WINDOWS_A35_INTERFACE.md` 接口 v2 定稿（双盲原则、42 函数全表、联动与锁存语义）。
- 测试规模扩展至 **11 套件 135 用例**（Debug/Release 双配置全绿）。

### 2026-09-01

- 终端 GUI 重构为 FluentUI Style（无边框、导航、告警中心），按协议文档对接全部命令。

## 2026-08-26 ~ 2026-08-29

### 视频链路与 AI

- CamStream 重写：修复色彩、帧率与内存问题，Phase 1 模块化重构（原 920 行 main 拆分为 `core/` 共享库），推流目标 IP 基线切换至 192.168.1.100。
- 终端接收加固（2MB 套接字缓冲、绑定校验回退、无包诊断）与真机黑屏/丢帧假象修复。
- AI 结果标注升级：类别名标签框（data.yaml 解析）、逐类别配色、检测信箱精确映射。
- Phase 7 验收：部署包脱 PATH 独立运行，满载性能达标（720p@30 零丢帧、单帧推理 ~3ms、整机 CPU ~1.5%）。

## 更早（2026-07 ~ 2026-08 中旬）

- 仓库建立与初始开发：Salacia_Terminal 初版（视频接收、遥测、遥控雏形）、CamStream 摄像头推流配置、通信协议设计。
- ROV_M33 固件里程碑（详见 `ROV_M33/project-docs/`）：
  - 硬件稳定基线 `v0.1.0`（OpenAMP/RPMsg + PCA9685 + MPU6500，实机验证）；
  - 裸机横滚/俯仰姿态稳定扩展台架验证（2026-08-30）；
  - A35 侧 RovControl API v1 与上电自检 `rov_self_test` 实机验收（2026-08-31）；
  - DYP-L08 UART 真机链路验证与 J4 跳线干扰根因定位（2026-08-31）。
