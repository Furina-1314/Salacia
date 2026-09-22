> 这是 **ROV A35/M33 Control Protocol v1.0** 当前正式协议基准版。

# ROV A35/M33 控制指令集规范 v1.0

基于已验证 OpenAMP/RPMsg 链路的 ROV 应用层控制协议

版本状态：v1.0 / `v0.1.0-hardware-baseline` 后第一版裸机 Roll/Pitch 姿态稳定台架验证实现

当前实现边界：统一四位 SEQ envelope、Servo、Propeller、`horizontal`、`synchronization`、锁存式 `stop`/`move`、MPU、DYP、sensor snapshot、姿态与 stabilization telemetry 均已由 M33 实现。A35 `RovControl API v1` 已覆盖这些当前 wire commands，完成 72/72 主机测试、Linux 板端构建和代表性 API 真机验收。此前的 CH10～CH13“以舵代推”验证仍只代表 **bench hardware validation passed; real thruster / underwater validation pending**。`ask`、版本/status/help、`safe`、`emergency` 和 `event` 当前未由 M33 实现，因此 A35 API 不暴露这些能力。

目的：在不改变既有 OpenAMP/RPMsg 基础设施的前提下，建立统一的 A35/Linux ↔ M33 应用层控制、状态回传与安全控制接口。当前固件为裸机实现，FreeRTOS 属于后续可选架构。

本版依据前两版草案完成收敛，用户命令统一小写、SEQ 由 A35/Linux 侧自动生成，并正式确定 stop / safe / emergency 三者语义。

## 1. 设计原则

- 复用已经验证的 /dev/ttyRPMSG0 通信链路，不重新设计异核通信底层。
- Linux/A35 发送业务意图，M33 负责具体硬件动作；Linux 不直接操作 M33 HAL。
- 命令协议采用可读 ASCII 文本，便于 SSH/Linux 命令行直接测试；后续如有性能需求可演进为二进制协议。
- 协议关键字、子命令、模式名和正式工具输出统一使用小写；M33 接收端允许大小写不敏感解析。
- SEQ 由 A35/Linux 侧的 ctl 自动递增，用户不手工填写；M33 对 ACK、错误和数据响应原样返回 SEQ。
- 运动控制必须具备明确的正常停止、保护模式和紧急脱险机制。
- 通信超时、看门狗、关键传感器异常或系统故障时，不得无限期保持最后一次危险推进器命令。
- 传感器和执行器通过服务层暴露能力，底层 GPIO/I2C/UART/PWM 实现对上层透明。

## 2. 系统结构

```text
A35/Linux shell
  -> /dev/ttyRPMSG0
  -> OpenAMP/RPMsg
  -> main.c VIRT_UART0_RxCpltCallback
  -> CommandService
       -> ActuatorService -> Servo -> PCA9685 -> I2C4
       -> PropellerService -> ActuatorService -> PCA9685 -> I2C4
       -> SensorService -> MPU6500 -> I2C8
       -> SensorService -> DYP -> UART4 RX / PB7 trigger
  -> response with the same optional SEQ
```

当前正式 A35 客户端结构为：

```text
RovControl
  -> RpmsgClient
     -> ITransport
        -> PosixTtyTransport

ResponseParser: 按预期响应类型严格校验 payload
SequenceAllocator: 分配/回绕四位 SEQ
pending map + single reader thread: 按 SEQ 分发响应
```

此客户端实现没有改变上述 M33 wire protocol。

## 3. 协议格式

### 3.1 用户命令

```text
<command> [args...]
```

示例：

```text
ctl ask
ctl status
ctl set servo 2 90
ctl sensor mpu
ctl stop
ctl emergency
```

### 3.2 Wire format

| 层级 | 格式 | 示例 | 说明 |
|---|---|---|---|
| 用户输入 | `<command> [args...]` | `set servo 2 90` | 用户无需 SEQ |
| ctl → M33 | `<seq> <command> [args...]` | `0002 set servo 2 90` | SEQ 自动生成 |
| M33 → A35 | `<seq> <result> [data...]` | `0002 ok` | ACK/错误/数据必须回带相同 SEQ |
| 主动事件 | `event <event> [args...]` | `event ready` | M33 主动产生，不要求 SEQ |

字段约定：关键字、子命令和模式名使用小写；数值与设备 id 使用十进制；字段之间允许一个或多个空白字符。

为便于底层手工调试，M33 应兼容无 SEQ 的直接输入；正式应用始终建议通过 ctl 发送。

SEQ 是统一的 RPMsg message envelope，不属于 Servo、Propeller 或 Sensor 的业务参数。M33 必须先从完整消息中提取可选的 4 位十进制 SEQ，再把同一个 payload 交给命令分发器；所有命令族使用完全相同的 envelope 规则和响应格式。

## 4. 系统命令

| 命令 | 用途 | 用户示例 | 典型返回 |
|---|---|---|---|
| `ask` | 测试 M33 是否在线 | `ctl ask` | `0001 online` |
| `ver` | 查询 M33 固件版本 | `ctl ver` | `0002 version 1.0` |
| `status` | 查询系统/任务/设备状态 | `ctl status` | `0003 ok ...` |
| `help` | 查询支持命令 | `ctl help` | `0004 ok ...` |

## 5. 安全与模式命令

### 5.1 stop / move — 锁存式运动许可

当前实现维护 `global_stopped`、`vertical_stopped`、`horizontal_stopped` 三个独立且叠加生效的锁存状态。`stop` 置位全局锁存并立即将 CH10～CH15 写为 0 command；`move` 只清除全局锁存，不清除分组锁存，也不恢复旧输出。分组命令仅影响对应通道。stop 是持续锁定而不是 reset；任意 `set propeller ...` 都不会自动解除 stop。stop 保留 base、individual、horizontal 和 synchronization 状态，锁存状态下被阻止的推进器 set 命令返回 `err safety`。

正式命令：

```text
ctl stop
ctl move
ctl stop vertical
ctl move vertical
ctl stop horizontal
ctl move horizontal
```

返回：

```text
0005 ok
```

### 5.2 emergency — 紧急上浮

定义：放弃当前正常任务，立即进入紧急脱险状态。

- 水平推进器：输出归零。
- 垂直推进器：进入受限的紧急上浮推力。
- 紧急上浮推力为“最大允许安全上浮推力”，不等同于电机/电调的物理最大值。
- 在上浮过程中继续保留 IMU 姿态修正，以抑制 Roll/Pitch；姿态修正不得使总体上浮能力失去。
- 进入 emergency 后，普通运动控制命令默认拒绝，直至系统按规定退出该状态。

用户命令：

```text
ctl emergency
```

建议返回：

```text
0006 ok emergency
```

### 5.3 safe — 安全保护模式

定义：safe 是系统保护状态，而不是单一运动动作。其目标是限制危险控制，而不是自动决定 ROV 上浮或下沉。

- 开启水平限制（horizontal control）。
- 检查每个推进器的“基准转速”是否处于安全范围；超出范围时，将基准值强制限制到对应安全阈值。
- 安全范围检查以推进器基准转速为准，而不是检查经过水平 PID 等实时微调后的最终真实转速。
- 因此，如果基准值为 1900 us，而因水平 PID 微调导致真实输出暂时达到 2000 us，不应因为该 2000 us 直接判定为越界并强行改变 PID 输出。
- safe 不自动等同于 stop，也不自动等同于 emergency；进入 safe 后具体推进器输出由当前控制策略继续管理，但危险命令应受到限制。
- 通信超时、看门狗、关键传感器异常或系统故障时，可自动进入 safe。

用户命令：

```text
ctl safe on
ctl safe off
```

建议返回：

```text
0007 ok safe_on
0008 ok safe_off
```

## 6. 水平控制命令

| 命令 | 功能 | 用户示例 | 典型返回 |
|---|---|---|---|
| `horizontal on` | 启用水平/姿态自动补偿；用户只调整所有推进器的基准转速，程序对单个推进器做微调 | `ctl horizontal on` | `ok horizontal_on` |
| `horizontal off` | 关闭水平自动补偿，允许手动单独调整各推进器 | `ctl horizontal off` | `ok horizontal_off` |

水平控制开启时，推进器命令中的 value 表示基准转速；实际 PWM 由基准值与控制算法的微调量共同决定。

当前启动默认 `horizontal on`。ON→OFF 不主动写 CH10～CH13，四路当前实际输出保持不变，并在垂直运动许可时把 `real` 同步到 individual vertical，随后 PID reset。OFF→ON 时先执行 `vertical_base = last_vertical_base` 并立即将 CH10～CH13 写为该基准值；下一次 50 Hz Control Tick 再由 PID 接管。

## 7. 舵机命令

| 命令 | 功能 | 说明 |
|---|---|---|
| `set servo <id> <angle>` | 控制单个舵机 | angle 为 0–180° |
| `set servo all <angle>` | 控制所有舵机 | angle 为 0–180° |
| `get servo <id>` | 查询单个舵机角度 | |
| `get servo all` | 查询所有舵机角度 | |
| `set servo <id> mid` | 单个舵机归中 | 等价于 `set servo <id> 90` |
| `set servo all mid` | 所有舵机归中 | |

当前 PCA9685 封装于 ActuatorService；协议层不直接暴露 I2C/PWM 寄存器。单路 Servo 命令只提交对应的一个 PCA9685 channel。

## 8. 推进器命令

| 命令 | 功能 | 说明 |
|---|---|---|
| `set propeller vertical base <value>` | 设置垂直推进器组目标 | horizontal on 时更新 `vertical_base`；off 时更新 CH10～CH13 individual 状态 |
| `set propeller vertical <id> <value>` | 设置单路垂直推进器 | id：10～13；仅 horizontal off 合法 |
| `set propeller horizontal base <value>` | 设置水平推进器组目标 | synchronization on 时更新 `horizontal_base`；off 时更新 CH14～CH15 individual 状态 |
| `set propeller horizontal <id> <value>` | 设置单路水平推进器 | id：14～15；仅 synchronization off 合法 |
| `get propeller <id> base` | 查询单个推进器基准转速 | |
| `get propeller <id> real` | 查询单个推进器实时转速 | |
| `get propeller all base` | 查询所有推进器基准转速 | |
| `get propeller all real` | 查询所有推进器实时转速 | |

`value` 是百分比 command，范围为 -100～+100：-100 为最大反转，0 为停止，+100 为最大正转。映射固定为 `PWM_us = 1500 + 5 * command`，即 -100/0/+100 对应 1000/1500/2000 us；不附加推进器死区、方向或 ramp。姿态 PID 在 command 域对 CH10～CH13 各附加最多 ±10 correction。

`vertical_base` 与 `horizontal_base` 相互独立。`horizontal on/off` 选择 CH10～CH13 使用 vertical base/PID 模式还是 individual 模式；horizontal on 时 individual vertical 返回 `err safety`。`synchronization on/off` 选择 CH14～CH15 使用同一 horizontal base 还是 individual 状态；synchronization on 时 individual horizontal 返回 `err safety`。

两条 base 命令在对应模式 on/off 时都合法：模式 on 时更新并输出 base；模式 off 时不修改保存的 base，而是把对应 individual 状态和输出统一设为 value。horizontal ON→OFF 保存当前 `vertical_base` 但不改变当前输出，并把 `real` 同步到 individual vertical；OFF→ON 恢复 `last_vertical_base` 并立即归到该 base，下一 Control Tick 才恢复 PID correction。

base 查询只在对应 base 模式启用时合法：CH10～CH13 要求 horizontal on，CH14～CH15 要求 synchronization on，否则返回 `err safety`；`get propeller all base` 要求两种 base 模式均启用。real 查询始终返回软件记录的当前最终输出 command，不读取 PCA9685 寄存器。

用户单路命令只提交对应的一个 PCA9685 channel。vertical base、horizontal base、模式切换、PID 和 stop 是多路语义，可批量提交 4、2 或 6 路。所有写操作均先构造 candidate 状态，硬件写成功后才提交软件状态；I2C/PCA9685 失败返回 `err io`。

旧的 `vertical ...`、`horizontal base ...`、`horizontal <id> ...`、`set propeller all <value>` 和 `set propeller all stop` 不属于正式命令集。

## 9. 传感器查询

| 命令 | 作用 | 典型数据 |
|---|---|---|
| `sensor mpu` | 读取 MPU6500 当前数据 | ax/ay/az/gx/gy/gz |
| `sensor dyp` | 主动触发一次新的 DYP 测量，完成后异步回复 | distance_mm |
| `sensor all` | 查询 M33 侧传感器状态和最近有效 DYP 缓存；不触发测量 | ready/state/valid/distance/age |
| `get attitude` | 按需查询姿态 | roll/pitch 使用 cdeg，含 ready |
| `get stabilization` | 按需查询稳定控制 | error/PID 使用 ×100 缩放整数，含 CH10～13、fresh、mode 和 stop latch |

M33 不按 10/20 ms 周期主动发送 telemetry。姿态超过约 30 ms 未更新时，PID reset；若垂直运动未被 stop 锁存，CH10～CH13 回到当前 `vertical_base`，否则保持 0。

`sensor dyp` 使用单个 pending request。命令收到后立即返回主循环，不阻塞 MPU、PID 或 RPMsg；测量完成后使用原始可选 SEQ 回复：

```text
0008 sensor dyp
0008 ok dyp distance_mm 742
```

无 SEQ 的手工命令同样延迟回复，但不会人为生成 SEQ。已有测量尚未结束时，新的 `sensor dyp` 返回 `err busy`。初始化不可用返回 `err not_ready`，响应超时返回 `err timeout`，UART error、错误帧头或 checksum 错误返回 `err io`。

`sensor all` 返回格式：

```text
ok sensors mpu <ready|not_ready> dyp <ready|not_ready> state <uninitialized|idle|waiting|complete|timeout|io_error> busy <0|1> valid <0|1> distance_mm <value|invalid> age_ms <value|invalid>
```

DYP 使用 UART4 受控输出模式：PB6/AF3 为 UART4_RX，PB7 为高电平空闲的 GPIO push-pull trigger，UART4 TX 不使用，UART4 不再作为 M33 BSP_COM debug 端口。串口为 115200/8N1，帧为 `FF Data_H Data_L SUM`，`SUM=(FF+Data_H+Data_L)&FF`，距离单位为 mm。第一版使用 UART RX interrupt 与非阻塞状态机，触发周期最小 34 ms，响应 timeout 60 ms。trigger LOW 目标为 5 ms，并使用一个 HAL tick 的量化裕量，因此当前实际约为 5-6 ms，主循环调度延迟只会延长该时间；大于 33 ms 是两次测距的触发周期约束，不是 trigger pulse width。

ATK板的J4可将CH342F Channel 1接入UART4网络：CH342F TXD1连接UART4_RX/PB6，CH342F RXD1连接原UART4_TX/PB7网络。UART4用于DYP时必须拔除这两个J4跳帽，使CH342F与PB6/PB7物理隔离。跳帽未拔时真实DYP请求已复现为timeout；拔除后，在固件、DYP接线、电平转换和UART配置均不变的条件下，M33成功收到真实DYP回包并返回65533 mm。该数值仅证明真实DYP UART回包链完成，不在本文中解释为有效距离。

当前姿态稳定实现参数：MPU6500/AttitudeEstimator 以 100 Hz 更新；启动需要 200 个成功样本计算三轴 gyro bias 和 accel startup reference；互补滤波 `alpha=0.98`。Roll/Pitch 双 PID 以 50 Hz 更新，初始参数均为 `Kp=0.8`、`Ki=0.02`、`Kd=0.15`，连续 deadband 为 ±10°，PID 输出及 Mixer 每路 correction 最大均为 ±10 command。Mixer 为：

```text
CH10 correction =  roll_pid - pitch_pid
CH11 correction =  roll_pid + pitch_pid
CH12 correction = -roll_pid - pitch_pid
CH13 correction = -roll_pid + pitch_pid
```

CH14～CH15 不参与姿态 PID。以上方向已通过 CH10～CH13 舵机台架动作及 `get stabilization` command 符号交叉验证，但尚未验证真实推进器方向和水下闭环动力学。

## 10. A35/Linux 侧设备

- INA226：由 Linux I2C3 + ina2xx + hwmon 管理；已实际验证 VBUS 约 4.955 V。
- DHT11：优先使用板卡已有 Linux dht11 驱动，待新硬件到货完成最终实机验证。

因此第一版 M33 协议不强制加入 power/temp 等 M33 硬件查询命令；A35/Linux 可以本地采集并由上层控制程序统一展示。

## 11. M33 → A35 主动事件

| 事件 | 用途 |
|---|---|
| `event ready` | M33 启动完成 |
| `event sensor_ready <name>` | 传感器初始化完成 |
| `event sensor_timeout <name>` | 传感器读取超时 |
| `event servo_error <id>` | 执行器异常 |
| `event safe_stop <reason>` | 系统进入安全保护状态 |

## 12. 返回码

| 返回码 | 含义 |
|---|---|
| `ok` | 命令已接受/执行 |
| `err bad_cmd` | 命令不存在 |
| `err bad_arg` | 参数错误 |
| `err busy` | 设备/任务忙 |
| `err not_ready` | 设备尚未就绪 |
| `err timeout` | 操作超时 |
| `err io` | 底层总线/UART、帧头或校验错误 |
| `err unsupported` | 暂未实现 |
| `err safety` | 当前安全状态不允许执行 |

## 13. 后续 FreeRTOS Task 规划（当前未实现）

| Task | 职责 | 建议优先级 |
|---|---|---|
| CommTask | 接收 RPMsg、解析命令、发送 ACK/数据 | 高 |
| ControlTask | 姿态/推进器控制、模式状态机 | 最高实时优先级 |
| SensorTask | MPU6500、DYP；DHT11 若最终留在 M33 则加入 | 按设备频率 |
| ActuatorTask | LU9685/PCA9685/舵机输出，统一仲裁执行器资源 | 高 |
| StatusTask | 周期状态与事件上报 | 低于控制任务 |

## 14. 软件分层

- 应用层：ROV 命令、模式、安全状态。
- 通信层：RPMsg/OpenAMP、消息收发。
- 服务层：SensorService / ActuatorService / ControlService。
- 驱动层：MPU6500 / LU9685 / DYP / GPIO / UART / I2C / PWM。

上层命令不直接调用 HAL；硬件替换时只需调整对应服务/驱动层，不改变 A35/Linux 命令接口。

## 15. A35/Linux RovControl API v1

`A35/rov_control` 是当前已实现的轻量 C++17 客户端。它生成 SEQ、独占 `/dev/ttyRPMSG0`、等待并按 SEQ 分发响应，再把 wire error 映射为类型化结果。公共 API 覆盖当前 M33 已实现的 Servo、Propeller、mode、stop/move、MPU、DYP、sensor all、attitude 和 stabilization telemetry。

当前 M33 不实现 `ask`、版本/status/help、`emergency`、`safe` 或主动 `event`，所以 API v1 刻意不提供这些调用。未来 CLI/网络层可以建立在 RovControl 之上，但不得凭协议规划章节假定未实现命令已经可用。

代表性真机验收包括 DYP、MPU、sensor snapshot、attitude、stabilization、servo query/set、basic 和 stop/move。`setServo` 已观察到真实舵机动作。该结果不表示推进器 setter 已逐项进行真实推进器/水下验收，也不改变 real thruster/underwater pending 的结论。

### 15.1 RPMsg tty 独占规则

RovControl 运行期间必须独占 `/dev/ttyRPMSG0`：

- 不得同时运行 `cat /dev/ttyRPMSG0`；第二个 reader 会抢走回复并使 API 表现为 timeout。
- 不得同时执行 `echo "..." > /dev/ttyRPMSG0`；第二个 writer 会破坏客户端 request/SEQ ownership。
- RPMsg tty response 不是广播。调试 shell 和正式 API 客户端必须串行使用，而不能共享同一 endpoint。

## 16. SEQ 规则

- SEQ 由 A35/Linux 侧生成，固定为 4 位十进制字符串：0000～9999。
- 每次成功发送一个命令后递增；超过 9999 后回绕到 0000。
- M33 对 ACK、错误和数据响应均原样返回相同 SEQ。
- 主动 event 消息不要求 SEQ。
- 直接手工写入 /dev/ttyRPMSG0 时可以省略 SEQ；M33 应兼容此调试模式，但正式程序推荐始终通过 ctl。

## 17. 通信与安全策略

- M33 必须能够检测 A35 控制通道超时。
- 超时不应无限保持上一条推进器命令。
- 进入 safe 时执行基于“推进器基准值”的安全限幅，并保持水平限制开启。
- 进入 emergency 时立即关闭水平推进器输出并执行受限紧急上浮控制。
- safe、stop、emergency 是三个不同语义：safe 是保护状态，stop 是正常停止动作，emergency 是主动脱险动作。
- 最终失联策略、超时时间、紧急上浮推力和安全 PWM 阈值应在实机浮力/推进器测试后固化为工程参数。

## 18. 当前硬件归属

| 执行环境 | 接口 | 设备 | 状态 |
|---|---|---|---|
| A35/Linux | I2C3 | PCF8563 | 板载 RTC，Linux 已管理 |
| A35/Linux | I2C3 | INA226 | ina2xx/hwmon；已读到约 4.955 V |
| A35/Linux | GPIO/driver | DHT11 | 已有 Linux 驱动；待新硬件验证 |
| M33/bare-metal | I2C8 | MPU6500 | 当前基线已验证 |
| M33/bare-metal | I2C4 | PCA9685 HW-170 | 当前 Servo/Propeller 基线已验证 |
| M33/bare-metal | UART4 RX + PB7 GPIO | DYP-L08 UART 受控输出版 | 真实UART回包已验证；使用前必须拔除UART4/CH342F的两个J4跳帽；有效距离语义/水下测距待验证 |
| 排除 | I2C7 | STPMIC2/system regulators | Secure CID1，排除出 ROV 应用路径 |

## 19. v1.0 第一阶段实现验收

- [ ] ask / response
- [ ] ver
- [ ] status
- [ ] help
- [x] stop / move（含 vertical/horizontal 独立分组锁存；实现与台架控制链已验证）
- [ ] emergency
- [ ] safe on / safe off
- [x] horizontal on / horizontal off
- [x] synchronization on / synchronization off
- [x] set/get servo
- [x] set/get propeller（不含已删除的 all setter）
- [x] get attitude / get stabilization（按需查询；台架验证）
- [x] roll/pitch PID + Mixer + 100/50 Hz 裸机调度（以舵代推台架验证；真实推进器/水下待验证）
- [x] sensor mpu
- [x] sensor dyp / sensor all（异步SEQ；假DYP 272 mm及真实DYP UART回包已验证；有效水下距离待验证）
- [ ] event ready / error
- [ ] 通信超时进入 safe 的机制

## 20. 后续可扩展项

- 将文本协议演进为固定结构二进制协议（仅在性能/带宽需要时）。
- 增加 heartbeat 命令或固定周期 heartbeat 消息。
- 增加舵机速度、限位、校准等参数。
- 增加姿态保持/自动任务相关控制命令。
- 使网络控制接口与 ctl 使用统一业务语义。
- 将 A35/Linux 侧 INA226、DHT11 数据统一接入高层 UI/状态监控。
