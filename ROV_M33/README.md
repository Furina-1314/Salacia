# ROV_M33

STM32MP257D-ATK 上 Cortex-M33 的 ROV 实时控制固件。当前稳定基线是裸机 OpenAMP/RPMsg + PCA9685 执行器 + MPU6500 传感器路径；PID、姿态闭环、Mixer、FreeRTOS 和 Qt 上位机尚未实现。

## 稳定基线

- 版本：`v0.1.0`
- Git 标签：`v0.1.0-hardware-baseline`
- CubeIDE 工程：`STM32CubeIDE/CM33/NonSecure`
- Build Configuration：`CA35TDCID_m33_ns_sign`
- 正式 ELF：`STM32CubeIDE/CM33/NonSecure/CA35TDCID_m33_ns_sign/ROV_M33_CM33_NonSecure.elf`
- 正式 ELF SHA-256：`752FFC8DED138CA3535235791DC79C4E893E9EE27105C76EB996D6C874ECA6CD`

此基线已由实机验证：Linux `remoteproc0` 启动、`/dev/ttyRPMSG0` 通信、Servo、Propeller、MPU6500、PCA9685 推进器输出链路，以及统一 CommandService/SEQ 解析。开始 PID 或上位机开发时应从该标签创建后续分支，不覆盖此标签。

## 当前执行链

```text
A35/Linux command
  -> /dev/ttyRPMSG0
  -> OpenAMP/RPMsg
  -> main.c receive callback
  -> CommandService (optional four-digit SEQ + payload)
  -> Servo / PropellerService / SensorService
  -> ActuatorService
  -> PCA9685
  -> I2C4
  -> Servo / ESC / Thruster
```

Servo 使用 CH0～CH9，角度范围为 0～180°。Propeller 使用 CH10～CH15，命令值为 -100～+100%，按 `PWM_us = 1500 + 5 * command` 映射到 1000～2000 us。单路 Servo/Propeller 命令只提交一个 PCA9685 channel；组命令、模式切换和 `all stop` 按语义提交相应通道组。

## 当前命令格式

所有当前命令均可直接发送 payload，或在前面加入四位十进制 SEQ：

```text
<payload>
<SEQ><whitespace><payload>
```

示例：

```sh
echo '0001 set servo 0 90' > /dev/ttyRPMSG0
echo '0002 set propeller vertical 10 -40' > /dev/ttyRPMSG0
```

当前执行器命令：

```text
set servo <id> <angle>
set servo all <angle>
get servo <id>
get servo all
set servo <id> mid
set servo all mid
set propeller vertical base <value>
set propeller vertical <id> <value>
set propeller horizontal base <value>
set propeller horizontal <id> <value>
set propeller all stop
get propeller <id> base
get propeller all base
get propeller <id> real
get propeller all real
horizontal on
horizontal off
synchronization on
synchronization off
sensor mpu
```

完整范围、模式、安全返回和当前实现边界见 `project-docs/ROV_A35_M33_Control_Protocol_v1.0.md`。

## 构建与部署

STM32CubeIDE 1.17.0 使用 `CA35TDCID_m33_ns_sign` 配置生成 ELF、stripped ELF 和签名 BIN。Linux `remoteproc0` 当前加载 ELF。每次部署必须先上传到临时路径、核对 SHA-256，再复制到：

```text
/lib/firmware/ROV_M33_CM33_NonSecure.elf
```

构建、部署、哈希校验和验证记录规则见 `project-docs/DEVELOPMENT.md` 与 `project-docs/DEVELOPMENT_LOG.md`。
