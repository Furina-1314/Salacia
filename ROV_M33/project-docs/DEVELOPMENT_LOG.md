# ROV_M33 Development Log

This log records durable implementation, artifact, deployment, and hardware-validation facts. Each stable entry binds the source version to the actual firmware artifact and board result.

## 2026-08-31 — Formal rov_self_test default mode passed real-board acceptance

- Added `rov_self_test` as a pure RovControl API v1 consumer. It has no direct tty I/O, ASCII command formatting, SEQ ownership, or M33 dependency beyond the frozen public API.
- Default checks query Servo, Propeller software output/base, MPU, DYP, sensor snapshot, attitude, and stabilization, then leave global stop latched. Mode-valid base-query `err safety` is SKIP; not-ready/fresh telemetry is WARN; transport/protocol/core-query and stop failures are FAIL.
- `--actuators` requires interactive `YES`, moves only CH9 by five degrees from its current software angle, verifies the reported software state, and restores it. Review confirmed that both setters use the same `RovControl::setServo()` path as smoke testing. No explicit delay exists between the test query and restore, so motion can be too small/brief to see. The software command/query/restore path is accepted; physical position is not automatically verified. The earlier `rov_api_smoke servo` board run remains separate evidence of clearly observed physical CH9 motion.
- `--thrusters` requires `I_UNDERSTAND` and sends no non-zero command. It verifies that global stop rejects a zero vertical-base setter with `err safety` and that all six software-recorded outputs are zero. This safety/state logic is host-validated and accepted; the optional mode was not separately run for this board acceptance and is not a real-thruster test.
- Cleanup performs best-effort `stop()` and `close()` on normal, failure, cancellation, and exception paths. A successful run intentionally requires a later explicit `move()` before vehicle motion.
- Added 18 host tests for pass/fail/warn/skip classification, critical failures, valid/invalid sensor cache, optional confirmations, servo restore, zero-only thruster safety, summary/exit mapping, cancellation, and cleanup. Clean MSVC `/W4 /WX` build and CTest passed; total suite 90/90 PASS.
- Default `./build/rov_self_test` passed on the real STM32MP257 board: RPMsg, all Servo state, Propeller output/base state, MPU6500, DYP, sensor snapshot, attitude, stabilization, and safe stop all passed. FAIL and WARN counts were zero; optional modes were SKIP, producing the accepted v1 summary `PASS WITH WARNINGS`. Global stop remained latched after exit.
- `rov_api_smoke` remains the developer diagnostic/fault-isolation tool. `rov_self_test` is the formal power-on/preflight tool. Real thruster direction, propulsion behavior, underwater dynamics, and closed-loop underwater control remain pending.

## 2026-08-31 — A35 RovControl API v1 completed and real-board validated

- Extended the validated MVP checkpoint `ccc3d50f60e98b16e8c5070c73459ec429358d46` to cover every command currently implemented by the M33 wire parser: Servo, Propeller, stabilization/synchronization modes, global and group stop/move, MPU, DYP, sensor snapshot, attitude, and stabilization telemetry.
- Preserved the architecture `RovControl -> RpmsgClient -> ITransport -> PosixTtyTransport`. One reader thread dispatches four-digit SEQ responses through the pending map; `SequenceAllocator` owns 0000-9999 allocation/wrap and `ResponseParser` strictly validates the response kind expected by each API.
- Kept `ask`, version/status/help, `emergency`, `safe`, and `event` out of the public API because current M33 firmware does not implement them. No M33 wire command or response was changed in this phase.
- Windows clean build passed with MSVC `/W4 /WX`; CTest passed and the self-contained host suite reported 72/72 PASS.
- Linux board build passed. Real-board smoke modes `dyp`, `mpu`, `sensors`, `attitude`, `stabilization`, `servo-get`, `servo`, `basic`, `stop`, and `move` passed. DYP repeated 5/5 and MPU repeated 3/3. `setServo` produced observed physical servo motion; complex sensor/attitude/stabilization replies were parsed successfully.
- DYP 65533 is retained only as proof of a valid real-sensor UART response, not as a physically valid distance.
- Validation scope is representative current-wire API and parser coverage. Real thruster direction, underwater dynamics, final PID tuning, and underwater closed-loop operation remain pending.
- A competing `cat /dev/ttyRPMSG0` process was confirmed to steal replies and cause an apparent `RpmsgClient` timeout. RovControl requires exclusive reader/writer ownership of the RPMsg tty; concurrent shell `cat` or `echo` is prohibited while it is active.

## 2026-08-31 — DYP-L08 UART4 real-sensor validation and J4 root cause

- DYP-L08 UART controlled output is configured for 115200/8N1; M33 uses PB6/AF3 for UART4_RX and PB7 as the GPIO trigger with an approximately 5-6 ms LOW pulse.
- An ESP32-S3 independently triggered and received the real DYP through the same 5 V/3.3 V level converter, proving that the sensor and converter path was operational.
- As a fake DYP, ESP32 detected the M33 PB7 trigger and injected `FF 01 10 10`; M33 returned 272 mm. This verified PB7 trigger generation, PB6/UART4_RX, UART4 IRQ, four-byte framing/checksum/distance parsing, delayed CommandService reply, SEQ, and RPMsg under a controlled input.
- With the real DYP connected directly to M33 and the UART4 J4 jumpers fitted, repeated `sensor dyp` requests timed out.
- J4 connects CH342F Channel 1 to the UART4 network: TXD1 to UART4_RX/PB6 and RXD1 to the UART4_TX/PB7 net.
- Removing both UART4 J4 jumpers physically isolated CH342F. With firmware, DYP wiring, level converter, and UART configuration otherwise unchanged, M33 received a real DYP response and returned 65533 mm.
- 65533 mm is retained only as evidence that the real DYP completed a UART response; no valid-distance meaning is inferred.
- Confirmed root cause: CH342F electrical interference through J4. Future DYP bring-up must begin by removing both UART4/CH342F J4 jumpers.

## 2026-08-30 — First bare-metal Roll/Pitch stabilization bench validation passed

The first Roll/Pitch stabilization version completed software tests, the formal signed build, and servo-substitute hardware bench validation. The formal status is: **bench hardware validation passed; real thruster / underwater validation pending**.

Traceability:

```text
Source identity:       commit containing this record
Parent commit:         3ad6064da9f49bf38b39b5f19975ab62587606ca
Commit message:        control: add bare-metal attitude stabilization
Build configuration:   CA35TDCID_m33_ns_sign
Formal ELF:            STM32CubeIDE/CM33/NonSecure/CA35TDCID_m33_ns_sign/ROV_M33_CM33_NonSecure.elf
ELF SHA-256:           5449DEB6FCEC22F26624C4EC2C629CFFE26E998CCEE3AF69DFBF085A7CB20FF2
ELF size:              text 47844 / data 360 / bss 4720
Host tests:            9/9 PASS (MSVC /W4 /WX)
Clean build:           PASS (0 errors, 0 warnings)
Board RPMsg:           PASS (/dev/ttyRPMSG0 and basic communication)
Bench control chain:   PASS
```

Bench validation used servos connected to PCA9685 CH10-CH13 in place of ESCs/thrusters:

```text
MPU6500
  -> AttitudeEstimator
  -> Roll/Pitch PID
  -> Vertical Mixer
  -> PropellerService
  -> ActuatorService
  -> PCA9685
  -> CH10-CH13 servos
```

- MPU attitude changes produced physical PWM/servo responses on CH10-CH13.
- `get stabilization` reported CH10-CH13 command signs matching observed servo motion.
- Roll/Pitch restoring directions matched the fixed Mixer design.
- The tested control configuration is 100 Hz MPU/attitude update, 200-sample startup gyro-bias/accelerometer-reference calibration, complementary-filter `alpha=0.98`, and 50 Hz Roll/Pitch control.
- Initial PID gains are `Kp=0.8`, `Ki=0.02`, `Kd=0.15`; the continuous deadband is +/-10 degrees; PID output and each mixed channel correction are limited to +/-10 command. CH14-CH15 do not participate.
- `horizontal` defaults ON. ON->OFF keeps CH10-CH13 unchanged, synchronizes individual vertical state from `real`, and resets PID. OFF->ON restores `last_vertical_base` immediately; the following Control Tick resumes PID.
- `stop`/`move`, vertical stop/move, and horizontal stop/move are independent persistent latches. Stop is not reset, blocked setters return `err safety`, and target/mode state is retained.

The initial hard-float artifact failed to create `/dev/ttyRPMSG0` because `main()` contained an FPU `vpush` before NonSecure CP10/CP11 had been enabled. The project now links its own `system_stm32mp2xx_m33_ns.c`; `SystemInit()` enables CP10/CP11 Full Access and executes DSB/ISB before `main()`. OpenAMP/VIRT_UART initialization was moved ahead of optional business services. MPU/Sensor/Attitude/Stability initialization failure now leaves the RPMsg diagnostic endpoint available with those functions unavailable/not-ready.

Known debt: I2C4/DMA still has fatal `Error_Handler()` startup paths; `HAL_I2C_ErrorCallback()` is not bus-specific; IMU stale/failure needs fuller fault injection; real ESC/thruster direction, underwater dynamics, and final PID tuning remain unverified. No new tag was created, and `v0.1.0-hardware-baseline` was not moved.

## 2026-08-30 — First bare-metal attitude stabilization implementation

- Preserved the horizontal transition repair in independent commit `3ad6064da9f49bf38b39b5f19975ab62587606ca`; the `v0.1.0-hardware-baseline` tag was not moved.
- Added 200-frame MPU6500 startup calibration, roll/pitch complementary filtering, independent roll/pitch PID, the fixed CH10-CH13 mixer, and StabilityControl.
- Added non-catching-up `HAL_GetTick()` scheduling: sensor/attitude at 100 Hz and control at 50 Hz, with OpenAMP polling opportunities between stages.
- Changed runtime MPU reads to one 14-byte burst with a 10 ms timeout; initialization retains the 500 ms timeout and existing ±2 g / ±250 dps configuration.
- Replaced the formal all-stop command with independent latch commands: `stop`/`move`, `stop vertical`/`move vertical`, and `stop horizontal`/`move horizontal`.
- A stale attitude older than 30 ms resets PID and restores CH10-CH13 to the current vertical base unless global/vertical stop requires zero.
- Added demand-only `get attitude` and `get stabilization`; there is no periodic RPMsg telemetry.
- MSVC host tests passed with `/W4 /WX`: PCA9685, Servo, PropellerService, MPU6500, AttitudeEstimator, PID, Mixer, StabilityControl, and CommandService.
- STM32CubeIDE 1.17.0 clean build of `CA35TDCID_m33_ns_sign` passed with 0 errors and 0 warnings. Size: text 47300, data 360, bss 4720. ELF SHA-256: `B4AD6225380A7BD17BC216ACBB6375740AC7A04D7A5B22792B92344ED5640C9A`.
- The ELF and signed image were generated locally only. No upload, remoteproc start, ESC/motor test, or board validation was performed.

## 2026-08-30 — v0.1.0 hardware baseline sealed

### Implementation completed before sealing

- Replaced the active LU9685 execution path with PCA9685 HW-170 on I2C4, using CH0-CH15.
- Unified Servo, Propeller, and Sensor message handling under CommandService with one optional four-digit SEQ envelope.
- Updated the formal Propeller namespace to `set propeller ...`; removed the old unprefixed setters and `set propeller all <value>`.
- Kept `set propeller all stop`; it sets all six real outputs to neutral while preserving mode, base, and individual configuration state.
- Unified individual Servo/Propeller execution so each individual command submits exactly one PCA9685 channel.
- Retained the separate `horizontal` and `synchronization` mode rules and `err safety` responses.

### Verification

- `remoteproc0` firmware start and `/dev/ttyRPMSG0` OpenAMP/RPMsg communication passed.
- Servo commands passed on hardware.
- Revised Propeller commands and PCA9685/ESC/thruster output chain passed on hardware.
- MPU6500 acquisition remained verified.
- Host regression tests and the existing CubeIDE build had passed before this archival task. The archival task did not clean or rebuild the project.

### ELF deployment incident and resolution

The first board behavior matched an older parser because the deployed ELF was the old baseline. Its SHA-256 was:

```text
82F0733FC673C279C7EA3563BAEF1551D62776F61CE79D298653A82728D1783B
```

The formal current ELF is:

```text
C:\Users\64135\Desktop\ROV_M33\STM32CubeIDE\CM33\NonSecure\CA35TDCID_m33_ns_sign\ROV_M33_CM33_NonSecure.elf
SHA-256: 752FFC8DED138CA3535235791DC79C4E893E9EE27105C76EB996D6C874ECA6CD
```

Resolution procedure:

1. Uploaded the intended ELF with MobaXterm to `/tmp/ROV_TEST_NEW.elf`.
2. Verified its SHA-256 on the board.
3. Copied that verified file to `/lib/firmware/ROV_M33_CM33_NonSecure.elf`.
4. Verified the final board file SHA-256 as `752FFC8DED138CA3535235791DC79C4E893E9EE27105C76EB996D6C874ECA6CD`.
5. Started remoteproc and verified that the revised commands worked.

Deployment rule established: after every upload, verify the temporary file hash before copying; verify `/lib/firmware/` again before starting `remoteproc0`.

### Traceability record

```text
Version:                 v0.1.0
Tag:                     v0.1.0-hardware-baseline
Source baseline commit:  d95a8883f6622b9ca6ec1da3c177711173ca3ddc
Build configuration:     CA35TDCID_m33_ns_sign
Formal ELF:              STM32CubeIDE/CM33/NonSecure/CA35TDCID_m33_ns_sign/ROV_M33_CM33_NonSecure.elf
Formal ELF SHA-256:       752FFC8DED138CA3535235791DC79C4E893E9EE27105C76EB996D6C874ECA6CD
Board ELF:               /lib/firmware/ROV_M33_CM33_NonSecure.elf
Board ELF SHA-256:        752FFC8DED138CA3535235791DC79C4E893E9EE27105C76EB996D6C874ECA6CD
Hardware status:          PASSED — RPMsg, Servo, Propeller, PCA9685/thruster, MPU6500
```

PID, attitude estimation, Mixer, Qt UI, sensor push/stream, and the production Linux command-line layer are not part of this baseline. PID/control development starts from the sealed tag.
