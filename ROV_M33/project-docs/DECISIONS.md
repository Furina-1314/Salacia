# Technical Decisions

## Use the user-provided ZIP as the Pre-RTOS code authority
The current **ROV_M33 Pre-RTOS Stable Baseline** was rebuilt solely from `C:\Users\64135\Desktop\ROV_M33.zip`. The failed RTOS working tree is not an implementation source for this baseline. Do not reintroduce FreeRTOS until a separately approved phase starts from this clean baseline.

## Keep the current baseline bare-metal
The application uses the ZIP baseline's `main` loop and `OPENAMP_check_for_message()` polling. Do not include CMSIS-RTOS, FreeRTOS tasks/kernel objects, an RTOS heap or port, or RTOS-specific SysTick/HAL-timebase changes in this baseline. The generic FreeRTOS support files retained inside upstream project-local libmetal are not application firmware inputs.

## Route Servo commands through ActuatorService
`main.c` and the command parser call `ActuatorService`, which owns the active hardware device handle and delegates Servo policy to the Servo layer. During the PCA9685 migration, ActuatorService owns the unique 16-channel pulse-width shadow; Servo owns only its 10 angle targets.

## Use the formal ROV_M33 tree
All active firmware development belongs in `C:\Users\64135\Desktop\ROV_M33`. External SDKs and verified examples are read-only references.

## Preserve the verified ATK/OpenAMP baseline
The formal project evolved from the verified `OpenAMP_TTY_echo_iic_LU9685` workflow. Keep the current OpenAMP/RPMsg, I2C4, signing, linker, and local-libmetal arrangements unless a task explicitly requires a change.

## Do not infer ATK routing from DK templates
STM32MP257F-DK material remains useful as a software reference, but DK pin mappings are not evidence for the ATK board without separate verification.

## Use Linux remoteproc
Use `remoteproc0` to deploy/start M33. This has been verified.

## Use the formal ROV ELF for remoteproc
The verified board workflow uses `ROV_M33_CM33_NonSecure.elf` because `remoteproc0/fw_format` is `ELF`.

## Do not repair vendor auto-loader yet
Manual ELF deployment works; do not make persistent BSP/device-tree/systemd changes until production-style automatic boot is actually required.

## Validate subsystems incrementally
Implement, build, and test one approved subsystem at a time. Do not combine later sensor, propulsion, control, or RTOS phases without approval.

## Keep propulsion business logic in PropellerService
Phase 4 / Step 1 places CH10–CH15 logical values, mode permissions, base/individual retention, mapping, and safe-neutral initialization in a standalone PropellerService. `main.c` only initializes the service and dispatches development commands. PID, attitude estimation, Mixer, and RTOS structures are deliberately deferred.

## Replace the LU9685 protocol instead of disguising PCA9685
PCA9685 is a register-based 16-channel PWM device, not an angle-command-compatible LU9685 variant. The active project therefore uses a new PCA9685 driver and removes LU9685, its 20-channel state, 0xFB/0xFD framing, and CH16-CH19 from the formal build. The legacy LU9685 source files are retained only as inactive historical source.

## Keep pulse width as the common actuator output domain
Servo maps 0-180 degrees to 500-2500 us. PropellerService maps logical command continuously with `1500 + 5 * command` to 1000-2000 us. ActuatorService and PCA9685 accept pulse widths; propulsion has no angle intermediate. No deadband clamp or software direction reversal is applied.

## Program safe outputs before starting PCA9685 PWM
ActuatorService supplies 1500 us for all 16 channels during initialization. PCA9685 programs MODE1 with SLEEP and auto-increment, MODE2, PRE_SCALE, and all LEDn PWM registers before clearing SLEEP. This ensures CH0-CH9 start at Servo midpoint and CH10-CH15 start at ESC neutral rather than using power-on full-off as an application command.

## Use the datasheet nominal oscillator for the migration candidate
The driver uses the PCA9685 datasheet nominal 25 MHz internal oscillator and target frequency 50 Hz. PRE_SCALE and pulse counts are derived from those configuration values. Oscillator tolerance and any board-specific calibration remain hardware-verification work.

## Supersede the temporary Phase 4 command namespace
The former unprefixed vertical/horizontal development setters are retired. The formal protocol now owns the `set propeller ...` namespace and the unified SEQ envelope.

## Superseded historical decisions
The former decisions to keep active work in `STM32CubeMP2-main` and defer creation of a formal ROV project are superseded. That directory is no longer an active project root.
## 2026-08-30 — Unify command envelope and Propeller write granularity

- SEQ is parsed once by a common `CommandService`; Servo, Propeller, and Sensor receive only the extracted payload.
- Formal Propeller setters use the `set propeller ...` namespace. Old unprefixed setters and `set propeller all <value>` are removed from the active command set.
- A user-level individual actuator command must produce a one-channel PCA9685 update. Group and stop commands retain 4/2/6-channel batch writes according to their semantics.
- Propeller configuration targets and current real outputs are distinct so all stop can neutralize hardware without resetting modes or saved targets.
- Control-mode prohibitions are `err safety`; actual PCA9685/I2C failures remain `err io` with the HAL error retained below the protocol boundary.

## 2026-08-30 — Seal the hardware-validated v0.1.0 baseline

- Use Git version `v0.1.0` and immutable tag `v0.1.0-hardware-baseline` as the sole starting point for subsequent PID/control development. The baseline binds source commit, CubeIDE configuration, ELF path/SHA-256, board deployment path/SHA-256, and hardware result.
- Keep Servo, Propeller, and Sensor under one `CommandService` envelope parser. Four-digit SEQ is transport metadata, is parsed once, and never becomes a business-service parameter.
- Preserve one-channel hardware submission for every individual Servo or Propeller setter. Only operations whose semantics cover a group may submit 2, 4, 6, 10, or 16 channels.
- Keep Propeller configuration targets separate from current real outputs. `set propeller all stop` is retained as the only all-channel setter and neutralizes six real outputs without deleting mode, base, or individual configuration state; `set propeller all <value>` remains removed.
- Require SHA-256 verification after upload to a board temporary path and again at `/lib/firmware/` before starting `remoteproc0`. A matching filename or successful start is not proof of firmware identity.

## 2026-08-30 — Implement the first bare-metal attitude stabilization chain

- Keep the implementation bare-metal: use rollover-safe 10 ms and 20 ms `HAL_GetTick()` gates and never catch up missed periods with a loop.
- Keep hardware ownership layered: AttitudeEstimator, PID, and Mixer cannot call ActuatorService/PCA9685; StabilityControl submits four command-domain values through `PropellerService_ApplyVerticalControl`.
- Use independent roll/pitch PID state with Kp 0.8, Ki 0.02, Kd 0.15, continuous ±10 degree thresholds, derivative on measurement, integral clamp ±5, conditional anti-windup, and output clamp ±10 command.
- Treat stop as a persistent permission latch, not a target reset. Global, vertical, and horizontal latches are independent; move clears only its matching latch and never restores an old real output.
- Preserve target/mode state across stop. Reject blocked setters with `err safety`; mode and get commands never clear a stop latch.
- On stale attitude, reset PID and restore the current vertical base only when vertical motion is permitted; otherwise preserve the stopped zero output.
- Keep the hardware-validated `v0.1.0-hardware-baseline` tag immutable. This control implementation needs separate board validation before any later stable tag.

## 2026-08-30 — Accept the first stabilization version at bench scope

- Classify the current milestone as **bench hardware validation passed; real thruster / underwater validation pending**. Servo-substitute CH10-CH13 testing validates the signal chain and Mixer directions, not ESC behavior, real propulsion direction, underwater dynamics, or final PID tuning.
- Keep the fixed Mixer signs: CH10=`roll-pitch`, CH11=`roll+pitch`, CH12=`-roll-pitch`, CH13=`-roll+pitch`, with every per-channel correction limited to +/-10 command. CH14-CH15 remain outside attitude PID.
- Make `horizontal` default ON. ON->OFF must not write CH10-CH13; it synchronizes individual vertical state from the current real outputs and resets PID. OFF->ON restores `last_vertical_base` immediately, with PID resuming on the next 50 Hz Control Tick.
- Keep `stop` as persistent motion permission rather than reset. Global and group latches compose independently; setters never clear stop, blocked setters return `err safety`, and base/mode state survives.
- In a hard-float NonSecure image, enable CP10/CP11 with DSB/ISB in the actually linked `SystemInit()` before calling `main()`; enabling FPU on the first C line of `main()` is too late when the prologue contains `vpush`.
- Create OpenAMP and VIRT_UART before optional Actuator/Sensor/Attitude/Stability initialization. Optional subsystem failures must remain diagnosable over RPMsg and expose not-ready/unavailable state. Core OpenAMP endpoint failure remains fail-stop.
- Preserve `v0.1.0-hardware-baseline` unchanged and do not create a new tag until real ESC/thruster and underwater validation justify one.
