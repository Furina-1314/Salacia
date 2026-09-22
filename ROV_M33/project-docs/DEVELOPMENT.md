# Development Workflow

## Build
Open `STM32CubeIDE/CM33/NonSecure` as `ROV_M33_CM33_NonSecure` in STM32CubeIDE 1.17.0. Build configuration: `CA35TDCID_m33_ns_sign`. Verify the final build and signing result, not just C compilation.

Expected main artifact:
`STM32CubeIDE/CM33/NonSecure/CA35TDCID_m33_ns_sign/ROV_M33_CM33_NonSecure.elf`

Expected signed artifact:
`STM32CubeIDE/CM33/NonSecure/CA35TDCID_m33_ns_sign/ROV_M33_CM33_NonSecure_sign.bin`

Current servo-substitute bench-validated control artifact:

```text
Build configuration: CA35TDCID_m33_ns_sign
ELF path:            STM32CubeIDE/CM33/NonSecure/CA35TDCID_m33_ns_sign/ROV_M33_CM33_NonSecure.elf
ELF SHA-256:         5449DEB6FCEC22F26624C4EC2C629CFFE26E998CCEE3AF69DFBF085A7CB20FF2
Build result:        0 errors / 0 warnings
Host tests:          9/9 PASS under MSVC /W4 /WX
Hardware result:     bench hardware validation passed; real thruster / underwater validation pending
```

## Deploy

The verified `v0.1.0` deployment procedure is explicit and hash-gated:

1. Upload the intended local ELF to `/tmp/ROV_TEST_NEW.elf` with MobaXterm.
2. Run `sha256sum /tmp/ROV_TEST_NEW.elf` on the board.
3. Continue only if the hash matches the local formal ELF.
4. Copy the verified temporary file to `/lib/firmware/ROV_M33_CM33_NonSecure.elf`.
5. Run `sha256sum /lib/firmware/ROV_M33_CM33_NonSecure.elf` before selecting or starting remoteproc.

For the hardware-validated baseline, both the local formal ELF and deployed ELF must equal:

```text
752FFC8DED138CA3535235791DC79C4E893E9EE27105C76EB996D6C874ECA6CD
```

Never infer firmware identity from a filename, timestamp, successful upload, or `remoteproc0=running`. Path plus SHA-256 is the deployment identity.

## Select firmware
```bash
echo ROV_M33_CM33_NonSecure.elf > /sys/class/remoteproc/remoteproc0/firmware
```

## Start
```bash
echo start > /sys/class/remoteproc/remoteproc0/state
cat /sys/class/remoteproc/remoteproc0/state
dmesg | tail -50
```

Expected state: `running`.

## Stop
```bash
echo stop > /sys/class/remoteproc/remoteproc0/state
```

## Validation discipline
For each feature: make one focused change -> build -> deploy -> smallest relevant test -> record verified result -> update PROJECT_STATE if the state changed.

Do not modify external SDKs or historical reference projects to make the formal ROV build pass. The libmetal pre-build source must remain `Middlewares/Third_Party/OpenAMP/libmetal` inside ROV_M33.

## Baseline traceability

Every stable baseline record must bind all of the following:

- Git version tag and exact source commit.
- STM32CubeIDE Build Configuration.
- final ELF full project-relative path and SHA-256.
- board deployment path and post-upload/post-copy SHA-256.
- explicit hardware validation result and tested command families.

Generated ELF/BIN/MAP/object files are not committed. Their identity is recorded by path and hash in `PROJECT_STATE.md` and `DEVELOPMENT_LOG.md`. The immutable source baseline is tagged; later PID/control work begins from that tag on a new branch or commit sequence.

## Current actuator commands

Servo, Propeller, and Sensor commands share one optional four-digit SEQ envelope. The parser removes `<SEQ><whitespace>` once, then dispatches the same payload rules for every command family. Direct input without SEQ remains supported.

```text
set servo <id> <angle>                         # id 0..9
set propeller vertical base <value>
set propeller vertical <id> <value>            # id 10..13
set propeller horizontal base <value>
set propeller horizontal <id> <value>          # id 14..15
stop | move
stop vertical | move vertical
stop horizontal | move horizontal
get propeller <id> base|real
get propeller all base|real
get attitude
get stabilization
horizontal on|off
synchronization on|off
```

Propeller values are logical percentages -100..+100, never PWM microseconds. Both base commands are legal in both corresponding modes. In off mode they update the group's individual targets without changing the saved base. Individual vertical commands require horizontal off; individual horizontal commands require synchronization off; mode violations return `err safety`. Invalid ids/values/syntax return `err bad_arg`; PCA9685 transport failure returns `err io`.

`horizontal` starts ON. ON->OFF does not write CH10-CH13; it preserves the current real output, synchronizes the individual vertical state to that real value when motion is permitted, and causes PID state to reset. OFF->ON restores `vertical_base` from `last_vertical_base`, immediately writes the restored base to CH10-CH13, and lets the next Control Tick resume PID correction.

`stop` is a persistent motion-permission latch, not reset. Global, vertical, and horizontal stop latches compose independently. A matching `move` clears only its latch. A blocked `set propeller ...` returns `err safety` and never implicitly clears stop. Base, individual, and mode state survive stop.

`Tests/command_service_test.c` covers unified SEQ handling, new stop/move commands, removed commands, safety errors, and on-demand queries. Propeller tests cover stop-latch priority, mode/base retention, the PID control entry point, write widths, and rollback. Dedicated MPU6500, AttitudeEstimator, PID, Mixer, and StabilityControl tests cover burst acquisition, calibration/filtering, continuous thresholds, anti-windup, mixing/clamps, mode/stop gating, stale fallback, and I/O failure. Existing Servo and PCA9685 tests remain regression coverage. Host tests are not linked into firmware.

## Phase 4 hardware-validated PCA9685 baseline

The active actuator hardware is PCA9685 HW-170 at 7-bit address `0x40` on the existing I2C4 bus. All 16 channels share 50 Hz. CH0-CH9 are Servo and CH10-CH15 are Propeller. The command and output chain is hardware-verified. The PCA9685 internal oscillator is configured as the datasheet nominal 25 MHz; precision oscillator calibration remains a separate task.

The formal protocol document contains the active Servo/Propeller command set, shared SEQ envelope, latch-based stop/move commands, and on-demand stabilization queries. The control chain remains bare-metal and uses `HAL_GetTick()` scheduling at 100/50 Hz. Attitude startup uses 200 successful frames for gyro bias and accelerometer reference; the 100 Hz estimator uses complementary-filter `alpha=0.98`. Roll/Pitch PID runs at 50 Hz with initial gains 0.8/0.02/0.15, a continuous +/-10 degree deadband, and +/-10 command PID/Mixer correction limits. CH14-CH15 are excluded from attitude PID.

The `v0.1.0-hardware-baseline` milestone remains unchanged. The subsequent PID/attitude/Mixer artifact with SHA-256 `5449DEB6FCEC22F26624C4EC2C629CFFE26E998CCEE3AF69DFBF085A7CB20FF2` has now passed first-round servo-substitute bench validation from MPU6500 through real PCA9685 PWM output. Do not describe this as real-thruster or underwater PID validation.

## Startup robustness requirements

- In the hard-float NonSecure build, FPU access must be enabled before `main()` because its function prologue may execute `vpush`. The project-local linked `SystemInit()` sets CPACR CP10/CP11 Full Access followed by DSB and ISB.
- Required startup order is HAL/IPCC/platform synchronization -> OpenAMP -> VIRT_UART endpoint/callback -> optional business modules.
- MPU6500/I2C8, SensorService, AttitudeEstimator, or StabilityControl initialization failure must leave RPMsg diagnostics alive, expose unavailable/not-ready state, and prohibit PID rather than entering a fatal loop.
- Core OpenAMP/VIRT_UART failure remains fail-stop. I2C4/DMA fatal initialization paths and the shared `HAL_I2C_ErrorCallback()` are known follow-up work.
