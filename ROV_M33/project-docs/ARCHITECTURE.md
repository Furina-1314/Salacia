# ROV_M33 Architecture

This document describes the current implementation and its ownership boundaries. The RPMsg, Servo, Propeller, PCA9685/I2C4, and MPU6500/I2C8 baseline paths are hardware-verified. The first bare-metal attitude-stabilization extension has additionally passed servo-substitute bench hardware validation; real ESC/thruster and underwater validation remain pending.

## Current execution baseline

The current development firmware extends the immutable **ROV_M33 v0.1.0 Hardware-Validated Stable Baseline**. Execution remains bare-metal:

```text
Reset_Handler
  -> NonSecure SystemInit: enable CP10/CP11, DSB, ISB
  -> main hard-float prologue
  -> HAL / IPCC / required platform synchronization
  -> OpenAMP/RPMsg initialization
  -> VIRT_UART endpoint and callback registration
  -> DMA / I2C4 initialization
  -> ActuatorService initialization
  -> PropellerService safe-neutral initialization
  -> I2C8 initialization
  -> SensorService initialization
  -> AttitudeEstimator and StabilityControl initialization
  -> while (1)
       -> OPENAMP_check_for_message()
       -> 100 Hz MPU6500/AttitudeEstimator tick
       -> OPENAMP_check_for_message()
       -> 50 Hz StabilityControl/PID/Mixer tick
       -> OPENAMP_check_for_message()
       -> CommandService envelope parsing, dispatch, and replies
```

No application FreeRTOS kernel, CMSIS-RTOS wrapper, RTOS task, queue, heap, port, or RTOS-specific HAL timebase is present. PID, attitude estimation, Mixer, stale handling, and scheduling are implemented in the polling loop; sensor push/stream, Qt UI, and the production A35 command-line layer remain unimplemented.

The hard-float build may emit FPU instructions in a C function prologue, including `vpush` before the first C statement in `main()`. The actual linked project-local NonSecure `system_stm32mp2xx_m33_ns.c` therefore grants CP10/CP11 Full Access and executes DSB/ISB in `SystemInit()` before `main()` is called. FPU enable must not be moved to the first line of `main()`.

OpenAMP/VIRT_UART is core startup infrastructure and remains fail-stop if it cannot initialize. Optional business initialization happens after the endpoint is available. I2C8/MPU6500, SensorService, AttitudeEstimator, and StabilityControl failures are logged and represented as unavailable/not-ready; they do not remove RPMsg diagnostics or enable PID. Actuator/PCA9685 and Propeller initialization errors are likewise exposed through not-ready/I/O status. I2C4/DMA low-level initialization still has fatal `Error_Handler()` paths and is tracked as technical debt.

## A35/Linux
Current/future responsibilities:
- camera capture/streaming
- networking
- high-level application logic
- Linux services
- Linux remoteproc firmware deployment
- `/dev/ttyRPMSG0` control and data endpoint

## M33
Current/future responsibilities:
- real-time control
- GPIO/UART/I2C
- Timer/PWM
- hardware-verified I2C4 PCA9685 control
- CH0–CH9 servo control
- I2C8 MPU6500 raw sensor acquisition (previously hardware verified)
- bare-metal roll/pitch attitude stabilization and optional future RTOS tasks

## Inter-core
Verified baseline:

```text
A35/Linux
  -> /dev/ttyRPMSG0
  -> OpenAMP/RPMsg
  -> main.c VIRT_UART0_RxCpltCallback
  -> CommandService
  -> Servo / PropellerService / SensorService
  -> ActuatorService
  -> PCA9685
  -> I2C4
  -> Servo / ESC / Thruster
```

## Command envelope and dispatcher

`CommandService` is the common message boundary for Servo, Propeller, and Sensor commands. It extracts an optional four-digit decimal SEQ plus one or more whitespace characters exactly once, dispatches only the remaining payload, and adds the same SEQ to every result. Business services never interpret SEQ as a device parameter. Unsequenced payloads remain supported for direct debugging.

## Servo path
A35/Linux -> `/dev/ttyRPMSG0` -> OpenAMP/RPMsg -> `main.c` -> CommandService -> ActuatorService -> Servo -> PCA9685 -> I2C4 -> servos.

`ActuatorService` remains the command-facing actuator boundary and owns the PCA9685 device handle plus the unique 16-channel pulse-width shadow. It prepares CH0-CH15 at 1500 us, passes those safe values into `PCA9685_Init`, and initializes the Servo angle state at 90 degrees. PCA9685 register details do not enter `main.c`, Servo, or PropellerService.

Servo retains CH0-CH9 policy, 0-180 degree validation, mid=90, set/get operations, and state update only after successful ActuatorService submission. Servo maps 0/90/180 degrees to 500/1500/2500 us. The PCA9685 driver alone maps pulse width to 12-bit count and owns MODE1, MODE2, PRE_SCALE, and LEDn register access.

PropellerService owns CH10-CH15 configuration state, six-channel `real` state, and independent global/vertical/horizontal stop latches. Individual setters submit one channel. Group stop operations submit 6/4/2 neutral channels; move operations only clear their latch and never restore output. Hardware success is required before state is committed. `PropellerService_ApplyVerticalControl` is the only PID output boundary and updates only CH10-CH13 `real` values without changing targets or modes.

CH0-CH9 belong to the Servo API and CH10-CH15 belong to PropellerService. PCA9685 has exactly 16 channels; CH16-CH19 do not exist in the active architecture. The retained LU9685 source directory is not an Eclipse project input and is not linked into the candidate ELF.

## Basic propulsion path

A35/Linux command -> RPMsg/OpenAMP -> `main.c` -> CommandService -> PropellerService -> ActuatorService -> PCA9685 -> I2C4 -> ESC PWM -> thruster.

PropellerService converts logical command -100..+100 directly to ESC pulse width 1000..2000 us using `1500 + 5 * command`. There is no angle intermediate, deadband clamp, or direction compensation.

Ownership and selection are:

- CH10–CH13: vertical thrusters; `horizontal_enabled=ON` selects `vertical_base` plus PID correction, OFF selects four individual values. Startup default is ON.
- CH14–CH15: horizontal thrusters; `synchronization_enabled=ON` selects `horizontal_base`, OFF selects two individual values.
- `horizontal_enabled` and `synchronization_enabled` are independent.
- `vertical_base` and `horizontal_base` are independent.
- Base and individual values are retained independently across mode switches.
- `horizontal` ON->OFF performs no hardware write, copies the current CH10-CH13 `real` outputs into the individual vertical state when motion is permitted, and resets PID through state synchronization; there is no output jump.
- `horizontal` OFF->ON restores `vertical_base = last_vertical_base`, immediately writes that base to CH10-CH13, and lets the next 50 Hz Control Tick resume PID correction.

ActuatorService provides a contiguous channel-pulse submission interface. PCA9685 auto-increment writes update the requested contiguous register range with one I2C transaction, and the 16-channel pulse shadow commits only after successful transport. Servo keeps a separate 10-channel angle target state because that is its public business domain.

At 50 Hz with the nominal 25 MHz oscillator, PRE_SCALE is 121 and the actual nominal refresh is approximately 50.029 Hz. Count conversion uses `round(pulse_us * oscillator_hz / ((prescale + 1) * 1,000,000))`, rather than a fixed 20 ms magic constant. Individual Servo and Propeller commands each result in one-channel PCA9685 submission; group, mode, initialization, stop, and PID operations retain their defined multi-channel semantics.

## MPU6500 sensor path
A35/Linux -> `/dev/ttyRPMSG0` -> OpenAMP/RPMsg -> `main.c` -> CommandService -> SensorService -> MPU6500 driver -> I2C8 -> MPU6500 raw accelerometer/gyroscope data.

I2C8 uses an independent HAL handle and blocking transfers without DMA or IRQ. Runtime acquisition is one 14-byte burst from `ACCEL_OUT` with a 10 ms timeout. AttitudeEstimator averages 200 successful startup frames to establish three-axis gyro bias and an accelerometer startup reference, then runs a 0.98 complementary filter at 100 Hz.

StabilityControl runs independent Roll/Pitch PID controllers at 50 Hz. Both use initial gains `Kp=0.8`, `Ki=0.02`, `Kd=0.15`, a continuous +/-10 degree deadband, and a +/-10 command output limit. The Vertical Mixer is:

```text
CH10 = vertical_base + clamp( roll_pid - pitch_pid, -10, +10)
CH11 = vertical_base + clamp( roll_pid + pitch_pid, -10, +10)
CH12 = vertical_base + clamp(-roll_pid - pitch_pid, -10, +10)
CH13 = vertical_base + clamp(-roll_pid + pitch_pid, -10, +10)
```

Every final command is also clamped to the Propeller command domain. CH14-CH15 are not inputs or outputs of attitude PID. StabilityControl submits the four commands only through `PropellerService_ApplyVerticalControl`. A 30 ms stale attitude resets PID and restores CH10-CH13 to `vertical_base` unless a global/vertical stop latch requires zero output.

The bench-verified physical path is `MPU6500 -> AttitudeEstimator -> PID -> Mixer -> PropellerService -> ActuatorService -> PCA9685 -> CH10-CH13 servos`. The observed servo directions matched `get stabilization` and the Mixer restoring signs. This is not evidence of real thruster direction or underwater closed-loop behavior.
