# Project State

Last updated: 2026-08-31

Documentation root: `project-docs/`

Team handoff and board startup SOP: [`ROV_RUNTIME_HANDOFF.md`](ROV_RUNTIME_HANDOFF.md). This is the single operator-facing guide for normal power-on, rebuild conditions, self-test, Qt/RovControl ownership, fixed board paths, and troubleshooting.

This state file was migrated from the retired `STM32CubeMP2-main` validation workspace. References to `Template_CM33_NonSecure`, `OpenAMP_TTY_echo`, and their board-side directories in historical milestone sections are retained as evidence of earlier experiments; they are not current writable project paths. The current formal project and workflow are `ROV_M33` as documented below.

## Current Status

### 2026-08-31 rov_self_test host-verified and default mode board-accepted

The formal A35 power-on self-test target is now implemented as an upper-layer consumer of the frozen RovControl API v1. It does not open, read, write, or poll `/dev/ttyRPMSG0` directly, does not format wire commands or allocate SEQ values, and changes neither M33 nor the wire protocol. The test seam is isolated in `SelfTestRunner`; production access remains `RovControl -> RpmsgClient -> ITransport -> PosixTtyTransport`.

Supported CLI:

```text
./rov_self_test
./rov_self_test --actuators
./rov_self_test --thrusters
./rov_self_test --all
./rov_self_test --device /dev/ttyRPMSG0
```

The default low-risk run opens RPMsg, queries all Servo state, Propeller real/base state, MPU, DYP, sensor snapshot, attitude, and stabilization telemetry, then issues `stop`. It sends no Servo or Propeller setter. A base query rejected with the mode-defined `err safety` is reported as SKIP rather than a fault; attitude not-ready and stale stabilization data are WARN conditions. Transport/protocol/query failures and failure to establish safe stop are FAIL conditions.

`--actuators` requires an interactive `YES`, applies a five-degree CH9 command relative to its current software state, queries that state, and restores the original angle. Static review found no logic defect: the path is `getServo(9) -> setServo(9, original +/- 5 degrees) -> getServo(9) -> setServo(9, original)`. There is no explicit hold delay, so the small movement may be too brief to see. The actuator software command/query/restore path is reviewed and accepted; physical movement remains a human-observation item and is not automatically verified by `getServo()`. Separately, the earlier `rov_api_smoke servo` board run produced clearly observed CH9 motion and remains independent evidence that the physical Servo API/RPMsg/M33/PCA9685/servo chain works.

`--thrusters` requires the exact interactive confirmation `I_UNDERSTAND`. This first version issues no non-zero thruster command: it establishes global stop, verifies a zero vertical-base setter is rejected with `err safety`, and confirms CH10-CH15 software outputs are all zero. Its safety/state logic is host-validated and accepted. This optional mode was not separately run during the final self-test board acceptance and does not claim real thruster, propulsion, or underwater behavior.

Normal and exception paths perform best-effort stop and close. SIGINT/SIGTERM handlers only set an atomic cancellation flag; cleanup occurs in normal C++ context. On successful completion, global stop intentionally remains latched, and the subsequent controller must explicitly call `move()` before motion. Exit codes are 0 for PASS/PASS WITH WARNINGS, 1 for self-test failure, and 2 for CLI usage error.

Host verification: clean MSVC `/W4 /WX` build PASS; CTest PASS; existing 72 tests plus 18 self-test tests = 90/90 PASS.

The default `./build/rov_self_test` completed real-board acceptance with RPMsg open, CH0-CH9 Servo state, CH10-CH15 Propeller output, Propeller base, MPU6500, DYP, sensor snapshot, attitude, stabilization, and safe stop all passing. The result contained zero FAIL and zero WARN; the optional actuator/thruster checks were SKIP, so the current summary text was `PASS WITH WARNINGS`. This wording is accepted for v1. Global stop remained latched after exit as designed.

Tool roles remain distinct: `rov_api_smoke` is the developer diagnostic/fault-isolation tool; `rov_self_test` is the formal power-on/preflight health check.

Operational preflight remains mandatory: RovControl must exclusively own `/dev/ttyRPMSG0` (no concurrent shell `cat` or `echo`), and DYP use requires both UART4/CH342F J4 jumpers removed. A DYP value such as 65533 proves a valid UART frame only, not a valid physical distance.

### 2026-08-31 A35 RovControl API v1 real-board validation passed

The Linux-side `A35/rov_control` library now provides full coverage of the wire commands currently implemented by M33. Its runtime path is:

```text
RovControl
  -> RpmsgClient
     -> ITransport
        -> PosixTtyTransport
```

`RpmsgClient` owns one reader thread, the pending-request map, four-digit SEQ allocation/dispatch, timeout handling, and late-response quarantine. `ResponseParser` validates the response shape expected by each API call. The public API covers Servo, Propeller base/individual/query operations, stabilization and synchronization modes, global/vertical/horizontal stop/move latches, MPU, DYP, sensor snapshot, attitude, and stabilization telemetry. It deliberately does not expose `ask`, version/status/help, `emergency`, `safe`, or `event`, because the current M33 does not implement those commands.

Verification is separated by evidence level:

```text
Windows host clean build:        PASS (MSVC /W4 /WX)
Host tests:                      72/72 PASS
Linux board build:               PASS
Representative real-board API:  PASS
Physical actuator observation:  PASS (setServo moved the connected servo)
```

Real-board smoke validation passed for `dyp`, `mpu`, `sensors`, `attitude`, `stabilization`, `servo-get`, `servo`, `basic`, `stop`, and `move`. DYP passed 5/5 repeated runs and MPU passed 3/3; sensor snapshot reported both devices ready with a complete cached DYP response; attitude and stabilization telemetry parsed all current fields. The returned DYP value 65533 is evidence of a valid real-sensor UART frame only and is not classified as a physically valid distance. These representative APIs and the complex query/parser paths are board-validated; this does not claim that real thrusters, underwater dynamics, or underwater closed-loop control have been validated.

`/dev/ttyRPMSG0` has single-stream ownership semantics. While RovControl owns it, no second process may `cat /dev/ttyRPMSG0` or write commands with shell `echo`: another reader can steal a response and cause an apparent client timeout, while another writer violates request/SEQ ownership. The previously observed MVP client timeout was reproduced as this competing-reader condition and is not a `RpmsgClient` defect.

This A35 milestone changes no M33 source or wire protocol. Its implementation checkpoint parent is `ccc3d50f60e98b16e8c5070c73459ec429358d46` (`a35: add validated RovControl MVP`); the final API v1 identity is the commit containing this record.

### 2026-08-31 DYP-L08 UART4 real-sensor validation passed

The DYP-L08 UART controlled-output path is now hardware verified at 115200/8N1. M33 uses PB6/AF3 as UART4_RX and PB7 as a GPIO trigger with an approximately 5-6 ms LOW pulse. The ESP32-S3 first verified the real DYP and level converter independently. It then acted as a fake DYP: after detecting the M33 trigger, it injected `FF 01 10 10`, which M33 parsed as 272 mm through UART4 IRQ, the asynchronous CommandService request, SEQ preservation, and RPMsg reply.

Direct M33-to-real-DYP testing initially timed out while the two UART4 jumpers on J4 remained fitted. J4 connects CH342F Channel 1 into the same nets: CH342F TXD1 to UART4_RX/PB6 and CH342F RXD1 to the former UART4_TX/PB7 net. Removing both UART4 J4 jumpers physically isolated CH342F; without any further firmware, wiring, level-converter, or UART configuration change, `sensor dyp` then returned 65533 mm from the real DYP. This value is recorded only as proof of a complete real-sensor UART response and is not classified here as a valid distance measurement.

Root cause: CH342F Channel 1 caused electrical interference while connected to the DYP UART/trigger network through J4. **Whenever UART4 is assigned to DYP, both corresponding J4 jumpers must be removed before power-on/testing.** This is a mandatory hardware preflight item; leaving either shared UART4 connection in place invalidates DYP communication testing.

### 2026-08-30 first bare-metal roll/pitch stabilization bench-validated version

The first bare-metal roll/pitch stabilization version has completed software tests, a formal clean build, and hardware bench validation using servos in place of real thrusters. The core control chain and Mixer directions passed. This is deliberately classified as **bench hardware validation passed; real thruster / underwater validation pending**.

The implemented control path is MPU6500 14-byte runtime burst acquisition -> AttitudeEstimator -> independent Roll/Pitch PID -> fixed CH10-CH13 Vertical Mixer -> PropellerService -> ActuatorService -> PCA9685. MPU/attitude updates run at 100 Hz after 200 successful startup samples establish gyro bias and the accelerometer startup reference. The complementary filter uses `alpha = 0.98`. StabilityControl runs at 50 Hz with a continuous +/-10 degree deadband and initial Roll/Pitch gains `Kp = 0.8`, `Ki = 0.02`, `Kd = 0.15`. PID output and each mixed channel correction are limited to +/-10 command. CH14-CH15 do not participate in attitude PID.

Verification identity:

```text
Source identity:       commit containing this record
Parent commit:         3ad6064da9f49bf38b39b5f19975ab62587606ca
Commit message:        control: add bare-metal attitude stabilization
Build configuration:   CA35TDCID_m33_ns_sign
ELF path:              STM32CubeIDE/CM33/NonSecure/CA35TDCID_m33_ns_sign/ROV_M33_CM33_NonSecure.elf
ELF SHA-256:           5449DEB6FCEC22F26624C4EC2C629CFFE26E998CCEE3AF69DFBF085A7CB20FF2
ELF size:              text 47844 / data 360 / bss 4720
Host tests:            9/9 PASS (MSVC /W4 /WX)
CubeIDE clean build:   PASS (0 errors, 0 warnings)
Bench validation:      PASS
```

Hardware bench path:

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

The verified ELF starts under `remoteproc0`, creates `/dev/ttyRPMSG0`, supports normal RPMsg command/diagnostic traffic, reads MPU attitude, and produces real PCA9685 PWM changes on CH10-CH13. Servo motion trends agree with the CH10-CH13 command signs reported by `get stabilization`, and Roll/Pitch restoring directions agree with the designed Mixer signs. This validates the first complete software-to-hardware control chain on the bench; it does not validate real ESC/thruster direction, underwater dynamics, final PID tuning, or closed-loop underwater performance.

Startup robustness is also verified in this artifact. The project-linked NonSecure `SystemInit()` enables CP10/CP11 Full Access and executes DSB/ISB before the hard-float `main()` prologue. OpenAMP and the VIRT_UART endpoint are created before Actuator, Sensor, Attitude, and Stability business initialization. MPU6500/SensorService/AttitudeEstimator/StabilityControl initialization failure therefore leaves RPMsg diagnostics available and the affected capability in unavailable/not-ready state.

Current technical debt and pending validation:

- I2C4/DMA low-level initialization still contains fatal `Error_Handler()` paths.
- The global `HAL_I2C_ErrorCallback()` does not yet distinguish I2C4 from I2C8.
- IMU stale/read-failure behavior needs fuller on-board fault-injection testing.
- Real ESC/thruster directions, underwater dynamics, and final PID parameters remain unverified.
- The immutable `v0.1.0-hardware-baseline` tag and its historical ELF identity remain unchanged; no new tag is created for this bench milestone.

- Board: 正点原子 STM32MP257D-ATK / ATK-DLMP257.
- Cortex-A35 runs Linux and is reachable over SSH at `root@192.168.137.10`.
- The current Cortex-M33 stable-baseline ELF is hardware-verified through Linux `remoteproc0`; start/stop and `running` state are verified.
- Linux creates `/dev/ttyRPMSG0` after the current M33 firmware starts, and end-to-end A35/M33 OpenAMP/RPMsg Virtual UART communication is **VERIFIED**.
- The current PCA9685/I2C4 Servo and six-thruster Propeller paths are **HARDWARE VERIFIED** from Linux command input through RPMsg/M33/CommandService/ActuatorService to physical output.
- I2C8 has now been experimentally verified end-to-end with a real external IMU: Linux maps `0x46040000.i2c` to `i2c-2`, PZ4/PZ9 are configured as I2C8 AF8, and M33 successfully reads an MPU6500 at 7-bit address `0x68`.
- The formal ROV development path has therefore progressed from platform/OpenAMP validation to verified I2C4 actuator control and verified I2C8 sensor acquisition.
- The current formal code baseline is **ROV_M33 v0.1.0 Hardware-Validated Stable Baseline**. Its historical reconstruction source was the user-provided `C:\Users\64135\Desktop\ROV_M33.zip`; subsequent PCA9685, CommandService, and Propeller revisions are part of the sealed baseline.
- The current firmware does **not** include application FreeRTOS, CMSIS-RTOS, RTOS tasks, kernel objects, heap implementation, CM33 RTOS port, or RTOS-specific HAL timebase changes.
- The 2026-08-30 unified CommandService/SEQ parser, revised Propeller command set, one-channel Servo/Propeller writes, safety rules, and state-preserving `set propeller all stop` behavior are board-accepted.
- Phase 4 six-thruster basic control remains hardware-verified. The subsequent AttitudeEstimator, Roll/Pitch PID, Mixer, scheduling, PropellerService submission, PCA9685 output, and RPMsg diagnostics have now passed first-round servo-substitute bench validation; real thruster and underwater validation remain pending. RTOS remains deferred.

## v0.1.0 Hardware-Validated Stable Baseline

```text
Version:                       v0.1.0
Git tag:                       v0.1.0-hardware-baseline
Source baseline commit:        d95a8883f6622b9ca6ec1da3c177711173ca3ddc
Build configuration:           CA35TDCID_m33_ns_sign
Formal ELF path:                STM32CubeIDE/CM33/NonSecure/CA35TDCID_m33_ns_sign/ROV_M33_CM33_NonSecure.elf
Formal ELF SHA-256:             752FFC8DED138CA3535235791DC79C4E893E9EE27105C76EB996D6C874ECA6CD
Board deployed path:            /lib/firmware/ROV_M33_CM33_NonSecure.elf
Board deployed SHA-256:         752FFC8DED138CA3535235791DC79C4E893E9EE27105C76EB996D6C874ECA6CD
Hardware acceptance:            PASSED
```

This tag is the immutable starting point for subsequent PID/control development. It identifies the source, CubeIDE build configuration, exact ELF hash, deployment target, and board-validation result as one traceable baseline.

Verified at this baseline:

- Linux `remoteproc0` and `/dev/ttyRPMSG0` OpenAMP/RPMsg round trip.
- Servo CH0-CH9 and Propeller CH10-CH15 through PCA9685 on I2C4.
- MPU6500 acquisition on I2C8.
- Common optional four-digit SEQ envelope for Servo, Propeller, and Sensor payloads.
- Formal `set propeller ...` namespace and -100..+100 percent command domain.
- Individual Servo/Propeller commands submit exactly one PCA9685 channel.
- `horizontal` and `synchronization` mode safety rules.
- `set propeller all stop` neutralizes all six real outputs while retaining configuration, mode, base, and individual targets.

Not implemented at this baseline: PID, attitude estimation, Mixer, FreeRTOS control tasks, Qt UI, sensor push/stream, and a production A35 command-line user layer. These are later milestones, not implicit capabilities of `v0.1.0`.

## 2026-08-30 Unified Command and Propeller Execution Revision

Status:

```text
Unified 4-digit SEQ envelope:    IMPLEMENTED
Formal Propeller command set:    UPDATED
Single Propeller channel write:  IMPLEMENTED
Batch base/mode/stop writes:     IMPLEMENTED
Safety error classification:     err safety
Host tests:                      PASSED (/W4 /WX)
CubeIDE clean build:             PASSED (0 errors, 0 warnings)
ELF size:                        text 42116 / data 360 / bss 4464
Board/deployment regression:     PASSED
```

`CommandService` now owns the one optional four-digit SEQ envelope and dispatches the extracted payload to Servo, Propeller, and Sensor handlers. Servo no longer parses the unmodified RPMsg message, so `0001 set servo 3 90` and `0002 set propeller vertical 10 40` use identical envelope and response rules. This path is hardware-verified in the `v0.1.0` baseline.

The formal Propeller setters are `set propeller vertical base`, `set propeller vertical <id>`, `set propeller horizontal base`, `set propeller horizontal <id>`, and `set propeller all stop`. The old unprefixed vertical/horizontal setters and `set propeller all <value>` are inactive. Base and real software queries are implemented without PCA9685 register reads.

PropellerService now commits a one-channel actuator range for an individual command, a four-channel range for vertical group operations, a two-channel range for horizontal group operations, and a six-channel range for initialization/all stop. Candidate business and real-output state is committed only after the corresponding actuator write succeeds. All stop changes only the six real outputs to command zero and preserves modes, base targets, and individual targets. `ActuatorService_GetLastHalError()` exposes the PCA9685 driver's retained HAL error; `main.c` logs it locally on actuator I/O failure while the host protocol remains `err io`.

## 2026-08-29 Phase 4 Hardware Migration - PCA9685 Candidate

Status:

```text
Candidate:               PCA9685 MIGRATION CANDIDATE
Hardware:                NOT VERIFIED
Address:                 0x40 (7-bit, previously confirmed by I2C scan)
Bus:                     I2C4 (unchanged)
Channels:                16 (CH0-CH15)
PWM target:              50 Hz
Nominal oscillator:      25 MHz
PRE_SCALE:               121
Clean build:             PASSED (0 errors, 0 warnings)
Propeller host test:     PASSED (/W4 /WX)
Servo host test:         PASSED (/W4 /WX)
PCA9685 host test:       PASSED (/W4 /WX)
Board/deployment test:   NOT PERFORMED (explicitly out of scope)
RTOS:                    DEFERRED
PID:                     NOT IMPLEMENTED
Attitude/Mixer:          NOT IMPLEMENTED
```

The active path is now:

```text
Servo angle 0..180 -> 500..2500 us --+
                                      +-> ActuatorService -> PCA9685 -> I2C4
Propeller -100..100 -> 1000..2000 us -+
```

Servo CH0-CH9 retains the existing `servo_set`, `servo_set_all`, `servo_get`, `servo_get_all`, `servo_mid`, and `servo_mid_all` behavior. PropellerService retains logical range, mode/state ownership, busy rules, base/individual retention, and rollback behavior. Its former command-to-angle stage was removed; the direct mapping is `pulse_us = 1500 + 5 * command`.

PCA9685 initialization sets SLEEP and auto-increment, explicitly configures MODE2 and PRE_SCALE, writes all 16 safe pulses, then starts the oscillator after the datasheet-required stabilization interval. CH0-CH9 therefore start at the 1500 us Servo midpoint and CH10-CH15 at the 1500 us ESC neutral. ActuatorService owns the unique 16-channel pulse shadow and commits it only after a successful contiguous I2C update. Servo separately owns its 10-channel angle target state.

At the nominal 25 MHz oscillator and PRE_SCALE 121, the calculated refresh is approximately 50.029 Hz. Counts use `round(pulse_us * 25,000,000 / (122 * 1,000,000))`; representative counts are 102/307/512 for 500/1500/2500 us and 205/307/410 for 1000/1500/2000 us.

The formal Eclipse project includes PCA9685 and the hardware-neutral Servo service. LU9685 sources remain in the repository as inactive historical source, but the `.project`, `.cproject`, regenerated object list, and ELF contain no LU9685 driver input or symbol. CH16-CH19 do not exist in the active channel model.

I2C4 pin/MSP/DMA/RIFSC/IRQ/timing code, OpenAMP/RPMsg, resource table, mailbox, MPU6500, SensorService, linker, signing, and the formal protocol document were not changed. No firmware was uploaded and remoteproc was not started.

## 2026-08-29 Phase 4 / Step 1 — Basic Six-Thruster Control

Status:

```text
PropellerService:       IMPLEMENTED / NOT HARDWARE-VERIFIED
Static mapping review:  PASSED
State test compilation: PASSED (-Wall -Wextra -Werror)
Clean build:            PASSED (0 errors, 0 warnings)
PID:                    NOT IMPLEMENTED
Attitude estimation:    NOT IMPLEMENTED
Mixer:                  NOT IMPLEMENTED
RTOS:                   DEFERRED
Board/deployment test:  NOT PERFORMED (explicitly out of scope)
```

`PropellerService` owns only propulsion business state for CH10–CH15. The sole 20-channel LU9685 shadow remains in `lu9685_channel_state.c`; no second LU9685 state was added. The output path is:

```text
development command parser
  -> PropellerService
    -> ActuatorService_SetChannelAngles(CH10, six integer angles)
      -> LU9685_CommitAll()
        -> existing I2C4 transport
```

CH10–CH13 form the vertical subsystem and CH14–CH15 form the horizontal-thruster subsystem. `vertical_base` and `horizontal_base` are independent. `horizontal_enabled` selects vertical base versus CH10–CH13 individual state. `synchronization_enabled` independently selects horizontal base versus CH14/CH15 individual state. Base and individual values never overwrite one another. ON→OFF stores `last_vertical_base`; OFF→ON restores it.

The startup state is command 0 for every thruster, producing conceptual ESC PWM 1500 us and LU9685 protocol angle 90 degrees. The mapping is continuous over command -100..+100: `PWM_us = 1500 + 5 * command`, then `angle = 90 + 0.45 * command`. Since LU9685 accepts integer angles, the implementation uses nearest-degree rounding with exact half degrees rounded toward the larger angle (`-50 -> 68`, `+50 -> 113`). No deadband clamp, direction compensation, PID headroom, ramp, watchdog, timeout, kill switch, or ESC arm/disarm state machine was added.

All six candidate angles are submitted in one existing LU9685 `0xFD` full-channel transaction. Channels outside CH10–CH15 are copied from the unique shadow state, so CH0–CH9 Servo targets and CH16–CH19 remain preserved. Propeller business state and the LU9685 shadow are committed only after successful transport.

The development commands are documented only in `DEVELOPMENT.md`; `ROV_A35_M33_Control_Protocol_v1.0.md` was not changed. No firmware upload, remoteproc start, ESC connection, motor connection, or board modification was performed.

## 2026-08-29 Pre-RTOS Stable Baseline Rebuild

Status:

```text
Pre-RTOS baseline:        SOFTWARE VERIFIED / HARDWARE PENDING RE-VERIFICATION
LU9685:                   PREVIOUSLY VERIFIED
MPU6500:                  PREVIOUSLY VERIFIED
ActuatorService refactor: IMPLEMENTED / NOT HARDWARE-REVERIFIED
FreeRTOS:                 NOT INCLUDED
Clean build:              PASSED (0 errors, 0 warnings)
```

The user-provided `ROV_M33.zip` was verified as the only available candidate baseline. Its application project contains the verified bare-metal OpenAMP polling loop, I2C4/LU9685/Servo, I2C8/MPU6500, and SensorService, but no application FreeRTOS/CMSIS/kernel/heap/port/timebase files or build links. References to FreeRTOS under the project-local libmetal source tree are upstream generic examples and platform support that are not part of the firmware object list.

The actuator path is now:

```text
main / command parser
  -> ActuatorService
    -> Servo
      -> LU9685
        -> I2C4
```

`ActuatorService` owns the single LU9685 device handle and preserves the verified startup sequence:

```text
MX_I2C4_Init()
ActuatorService_Init()
  -> LU9685_Init()
  -> LU9685_Reset()
  -> servo_init()
  -> servo_set_all(90)
```

The driver, Servo policy, and single 20-channel shadow state were not changed from the ZIP baseline. The command parser retains its original matching, validation, replies, and `sensor mpu` path; only direct `servo_*` calls were replaced by equivalent `ActuatorService_*` calls. OpenAMP, I2C MSP/IRQ, SensorService, MPU6500, LU9685, Servo, and the protocol document are byte-identical to the ZIP baseline.

STM32CubeIDE 1.17.0 clean-built `CA35TDCID_m33_ns_sign` and generated:

```text
ROV_M33_CM33_NonSecure.elf
ROV_M33_CM33_NonSecure_stripped.elf
ROV_M33_CM33_NonSecure_sign.bin

text = 39476
data = 360
bss  = 4392
total = 44228 bytes
```

The final object list includes `main.o`, `lu9685.o`, `lu9685_channel_state.o`, `servo.o`, `actuator_service.o`, `mpu6500.o`, and `sensor_service.o`, and contains no FreeRTOS kernel objects. ELF inspection confirms the expected OpenAMP, LU9685, Servo, ActuatorService, MPU6500, and SensorService symbols and call relationships.

No board was connected for this rebuild. The previously verified subsystem facts remain historical evidence; this newly reconstructed/refactored artifact still requires board regression of `remoteproc0`, `/dev/ttyRPMSG0`, Servo commands, `sensor mpu`, and simultaneous I2C4/I2C8 operation.

## Historical Verified Platform Milestones

### Historical OpenAMP reference build

- Vendor package: 正点原子 `STM32Cube_ATK_FW_MP2_V1.1.0`.
- Project:
  `Projects/STM32MP257D-ATK/Applications/CM33_OpenAMP_DEMO/OpenAMP_TTY_echo`
- Build configuration: `CA35TDCID_m33_ns_sign`.
- Final build result: **0 errors, 0 warnings**.
- Generated artifacts:
  - `OpenAMP_TTY_echo_CM33_NonSecure.elf`
  - `OpenAMP_TTY_echo_CM33_NonSecure_stripped.elf`
  - `OpenAMP_TTY_echo_CM33_NonSecure_sign.bin`
- The working build uses CMake `3.31.12`.
- CMake `4.4.2` was incompatible with the older libmetal `CMakeLists.txt`; switching to CMake `3.31.12` resolved the build issue.

### Historical reference deployment and M33 startup

- The firmware has been started successfully with `fw_cortex_m33.sh`.
- Script location on the board:
  `/home/root/OpenAMP_TTY_echo/fw_cortex_m33.sh`
- Firmware actually started:
  `OpenAMP_TTY_echo_CM33_NonSecure.elf`
- Verified `remoteproc0` state: `running`.

### OpenAMP/RPMsg Virtual UART

- Linux successfully creates `/dev/ttyRPMSG0` after firmware startup.
- Manual verification used two SSH terminals:
  - Receive: `cat /dev/ttyRPMSG0`
  - Send: `echo "Hello alientek!" >/dev/ttyRPMSG0`
- The receive terminal displayed `Hello alientek!`.
- Therefore the following round-trip path is **VERIFIED**:

  `A35/Linux -> RPMsg/OpenAMP -> M33 -> RPMsg/OpenAMP -> A35/Linux`

- This proves that the M33 executes and that the formal A35/M33 communication link is operational.

## I2C4 / LU9685 Verification

### I2C4 bus discovery

- A dedicated I2C4 experiment was created on the formal M33/OpenAMP platform.
- The initial I2C4 scan reported three responding addresses:
  - `0x11` (HAL address `0x22`)
  - `0x22` (HAL address `0x44`)
  - `0x50` (HAL address `0xA0`)
- These were existing devices on the bus and were not the expected LU9685 address.

### LU9685 address identification

- The LU9685 module initially reported a PCB label of `0x80`, but probing 7-bit `0x40` / HAL `0x80` did not acknowledge.
- Because the module's address is selected from a limited five-bit address field, a 32-candidate scan was performed.
- The scan found:
  - `7-bit 0x00` / HAL `0x00`
  - `7-bit 0x11` / HAL `0x22`
- Linux inspection showed `0x11` was already associated with the existing `ES8388` device, so the LU9685 candidate was isolated as `0x00`.
- A direct M33 probe then produced:
  - `LU9685 probe 0x00: ACK`
- A software reset was also accepted:
  - `LU9685 reset: HAL_OK`

### LU9685 servo-control result

- The LU9685 was subsequently driven from Linux through `/dev/ttyRPMSG0`.
- Servo PWM range used for the experiment: approximately `500–2500 us`.
- The final command path was verified sufficiently for a Linux command carrying a target angle to cause the connected servo to move.
- This establishes the following verified chain:
  `A35/Linux command -> RPMsg/OpenAMP -> M33 -> I2C4 -> LU9685 @ 0x00 -> servo PWM -> physical servo motion`
- This is the current actuator-control reference for the ROV project.

## I2C8 / MPU6500 Verification

### Linux-side I2C8 identification

- Linux exposes the controller at `0x46040000` as:
  `i2c-2   STM32F7 I2C(0x0000000046040000)`
- Device-tree status for `46040000.i2c` is `okay`.
- The platform device is bound to:
  `/sys/bus/platform/drivers/stm32f7-i2c`
- Pinctrl inspection confirmed:
  - `PZ4 -> device 46040000.i2c function af8 group PZ4`
  - `PZ9 -> device 46040000.i2c function af8 group PZ9`
- Therefore the verified Linux-side mapping is:
  `I2C8 -> 0x46040000 -> Linux i2c-2 -> PZ4/PZ9 -> AF8`.

### Resource-manager investigation

- On M33, the following resource requests succeeded:
  - I2C8 RIFSC resource `48`
  - GPIOZ9
  - GPIOZ4
- `RCC resource 101` continued to report `FAILED`.
- Importantly, the M33 test was modified so that RCC 101 failure did **not** abort I2C8 initialization.
- Under that condition:
  - `HAL_I2C_Init` succeeded.
  - I2C8 bus scanning succeeded.
  - A real MPU6500 device was detected.
  - Register writes and multi-byte register reads succeeded.
- Therefore, based on the verified experiment, `RCC 101 FAILED` is **not a demonstrated blocker for practical I2C8 communication**. It remains a resource-ownership/configuration issue worth documenting, but it is no longer considered the reason I2C8 is unusable.
- Linux runtime PM was also inspected:
  - Device-tree status: `okay`
  - Runtime PM initially: `suspended`
  - Explicit `power/control=on` made it `active`
- Keeping Linux I2C8 active was useful as a controlled test condition, but the successful M33 I2C8 communication is the stronger evidence that the peripheral path itself is functional.

### MPU6500 identification and sensor readout

- A real sensor connected to I2C8 responded at 7-bit address `0x68`.
- Register `0x75` (`WHO_AM_I`) returned `0x70`.
- This result is consistent with the MPU6500 identification used for the current test, rather than the originally assumed MPU6050 `WHO_AM_I` value.
- The final diagnostic firmware completed the full sequence:
  1. Probe `0x68`
  2. Read `WHO_AM_I`
  3. Write `PWR_MGMT_1 = 0x00`
  4. Write `SMPLRT_DIV = 0x07`
  5. Write `CONFIG = 0x00`
  6. Write `GYRO_CONFIG = 0x00`
  7. Write `ACCEL_CONFIG = 0x00`
  8. Read six accelerometer bytes from `0x3B`
  9. Read six gyroscope bytes from `0x43`
- A successful run produced:
  - `WHO_AM_I = 0x70`
  - `ACCEL RAW: X=-12400 Y=832 Z=-10828`
  - `GYRO RAW: X=-571 Y=114 Z=-126`
  - `MPU6500 TEST: PASS`
- This establishes the following verified sensor path:
  `A35/Linux command -> RPMsg/OpenAMP -> M33 -> I2C8 -> PZ4/PZ9 -> MPU6500 @ 0x68 -> register configuration/readout -> accelerometer/gyroscope data`
- This is now the current sensor-side reference for the ROV project.

## Reproducible Formal ROV_M33 Workflow

This is the canonical workflow for rebuilding and validating the current formal M33/A35 baseline. The older `OpenAMP_TTY_echo` and DK Template paths remain historical references only.

### 1. Build on Windows

Project root:

`C:\Users\64135\Desktop\ROV_M33`

CubeIDE project:

`C:\Users\64135\Desktop\ROV_M33\STM32CubeIDE\CM33\NonSecure`

Project and build configuration:

```text
ROV_M33_CM33_NonSecure/CA35TDCID_m33_ns_sign
```

Expected artifacts:

- `ROV_M33_CM33_NonSecure.elf`
- `ROV_M33_CM33_NonSecure_stripped.elf`
- `ROV_M33_CM33_NonSecure_sign.bin`

The build must continue using the project-local libmetal source at `Middlewares/Third_Party/OpenAMP/libmetal`. External SDKs and reference projects are read-only.

### 2. Prepare and start the board-side firmware

Upload the selected local ELF with MobaXterm to `/tmp/ROV_TEST_NEW.elf`. Verify its SHA-256, copy that verified file to `/lib/firmware/ROV_M33_CM33_NonSecure.elf`, and verify the final hash again. For `v0.1.0`, both hashes must be:

```text
752FFC8DED138CA3535235791DC79C4E893E9EE27105C76EB996D6C874ECA6CD
```

Before starting, verify that `/sys/class/remoteproc/remoteproc0/fw_format` is `ELF`, select `ROV_M33_CM33_NonSecure.elf` through the remoteproc firmware sysfs node, and start `remoteproc0`.

Verify after startup:

```bash
cat /sys/class/remoteproc/remoteproc0/state
ls -l /dev/ttyRPMSG0
```

Expected state: `running`, with `/dev/ttyRPMSG0` present.

### 3. Verify the formal RPMsg command path

Configure the endpoint as required:

```bash
stty -onlcr -echo -F /dev/ttyRPMSG0
```

The current formal acceptance commands include:

```bash
echo "0001 set servo 0 90" > /dev/ttyRPMSG0
echo "0002 set propeller vertical 10 -40" > /dev/ttyRPMSG0
echo "0003 sensor mpu" > /dev/ttyRPMSG0
```

The replies carry the matching SEQ and successful commands return `ok` or sensor data. This establishes the active formal path:

`A35/Linux -> /dev/ttyRPMSG0 -> OpenAMP/RPMsg -> main.c -> CommandService -> business service -> ActuatorService/PCA9685 or SensorService/MPU6500 -> response`

### Baseline Rules

- Active development occurs only in `C:\Users\64135\Desktop\ROV_M33` unless explicitly authorized otherwise.
- Do not recreate the M33/OpenAMP/remoteproc setup from scratch.
- Preserve the verified OpenAMP/RPMsg, I2C4/PCA9685, I2C8/MPU6500, linker, signing, and local-libmetal infrastructure.
- Treat `OpenAMP_TTY_echo` and `Template_CM33_NonSecure` only as read-only historical references.
- UART4/COM11, `/dev/mem` heartbeat, and ESP32 UART listening are not formal acceptance paths.

## Historical Platform Baseline (Superseded by ROV_M33)

The earlier platform-validation baseline was:

- Package: 正点原子 `STM32Cube_ATK_FW_MP2_V1.1.0`
- Board project family: `STM32MP257D-ATK`
- Application:
  `Projects/STM32MP257D-ATK/Applications/CM33_OpenAMP_DEMO/OpenAMP_TTY_echo`
- Communication architecture: Cortex-M33 and Cortex-A35 using OpenAMP/RPMsg Virtual UART.
- Runtime Linux endpoint: `/dev/ttyRPMSG0`.

The following ST template is retained only as an early bring-up and troubleshooting project and is **not** the formal M33 baseline:

- Package tree: `STM32CubeMP2-main`
- Project:
  `Projects/STM32MP257F-DK/Templates/Template_CM33_NonSecure`

Do not describe the STM32MP257F-DK template as the current formal solution, and do not use DK board pin mappings as evidence for the ATK board without separate hardware verification.

## Known Non-Blocking Issues / Abandoned Debug Paths

### UART4 / COM11 heartbeat

- Known hardware mapping from earlier investigation:
  - M33 UART: UART4
  - UART4 TX: PB7 / AF3
  - UART4 RX: PB6 / AF3
  - UART4 connects to CH342F Channel 1 / Windows COM11.
- No `M33 alive` heartbeat was observed on COM11.
- UART4 heartbeat is **NOT VERIFIED** and is not part of the formal project baseline.
- It is no longer a prerequisite for proving M33 execution because the formal OpenAMP/RPMsg link has been verified.
- Do not prioritize COM11 heartbeat investigation unless a future task specifically requires physical UART4 output.

### Shared-memory `/dev/mem` experiment

- An experiment attempted to read `IPC_SHMEM` through Linux `/dev/mem` to observe an M33 heartbeat.
- Linux returned `Bad address`.
- This path is **ABANDONED** and is not part of the formal solution.
- Do not prioritize further `/dev/mem`/`IPC_SHMEM` heartbeat investigation.

### ESP32-S3 UART listener

- ESP32-S3 was used as a side-channel listener on PB7/UART4_TX.
- ESP32 self-loopback was verified working.
- It did not receive an M33 UART4 heartbeat.
- This experiment is not part of the current development direction and must not be interpreted as failure of the formal M33/A35 platform link.

## Historical Development Direction Before Formal ROV_M33

Basic M33 execution and A35/M33 communication are complete. Two concrete peripheral paths are now experimentally verified:

1. I2C4 -> LU9685 -> servo actuator
2. I2C8 -> MPU6500 -> accelerometer/gyroscope data

At that stage, the proposed ROV development sequence was:

1. Consolidate the verified I2C4 actuator path.
2. Consolidate the verified I2C8 sensor path.
3. PCA9685 / multi-channel actuator expansion where required by the final ROV architecture.
4. A35/M33 control interface.
5. IMU data transport and attitude-estimation pipeline.
6. Multi-servo control.
7. Integrated ROV control logic.

UART4 heartbeat, COM11 monitoring, ESP32 UART listening, and `/dev/mem` shared-memory reads remain non-prerequisites.

## Historical Recommended Milestones (Pre-ROV_M33)

### 1. I2C8 sensor validation beyond one-shot reads

- Repeat MPU6500 reads after physically changing the module orientation.
- Confirm accelerometer raw values change consistently with gravity projection.
- Confirm gyroscope raw values respond to rotation and settle toward small values when stationary.
- Keep the diagnostic firmware's step-by-step logging while moving from bring-up to a reusable sensor driver.

### 2. Consolidate I2C4 actuator control

- Preserve the known-good LU9685 address `0x00` and servo-control command path.
- Document the exact Linux command format and PWM/angle mapping in the project code.
- Use this as the actuator reference when integrating the ROV control layer.

### 3. Keep I2C8 RCC/resource issue documented

- Do not block ROV development on `RCC resource 101 FAILED`.
- The current evidence shows that practical I2C8 communication works despite that request failure.
- Revisit resource ownership only if a future production configuration requires explicit ownership of the RCC resource or if another I2C8 operation exposes a reproducible failure.
## 2026-08-27 Progress Update

### I2C resource and architecture investigation

- I2C1~I2C8 were systematically surveyed at the Linux device-tree / RIFSC / board-pin / CM33-project level.
- I2C7 was traced to RIFSC resource `47` and the actual runtime security policy was confirmed from the OP-TEE embedded secure DT inside `fip-a`:
  - `I2C7 @ 0x40180000`
  - `SEC + PRIV`
  - static `CID1`
  - CID filtering enabled
  - semaphore disabled
- The M33 firmware is `NonSecure / CID2`, so `ResMgr_Request(I2C7)` deterministically fails at the security check with `NSEC_ACCESS_ERROR`.
- The same secure DT also contains `stpmic2@33` and system regulators under I2C7, confirming that I2C7 is a Secure PMIC/power-management bus rather than an appropriate application bus for the ROV.
- No official end-to-end `STM32MP257 + CM33 NonSecure + I2C7` application example or supported A35-Secure/CM33-NonSecure shared-I2C7 solution was found.
- Decision: **I2C7 is removed from the ROV application path**. It remains a platform/security investigation topic only.

### I2C4 / I2C8 application paths

- I2C4 was experimentally verified for `LU9685 @ 0x00` servo control through:
  `A35/Linux -> RPMsg/OpenAMP -> M33 -> I2C4 -> LU9685 -> servo`.
- I2C8 was experimentally verified for a real `MPU6500 @ 0x68`, including `WHO_AM_I = 0x70`, register configuration, accelerometer and gyroscope reads.
- Current architectural decision:
  - **I2C8 = primary M33 application bus**
  - **I2C4 = backup M33 application bus**
- I2C3 is not selected as the M33 bus because it is already a Linux-managed RTC bus and had previously produced an abnormal-restart / IAC fault during M33-side experimentation.

### I2C3 / INA226: alternative A35-side path verified

- Linux exposes I2C3 as `i2c-0`, corresponding to controller `0x40140000`.
- Existing Linux device:
  - `PCF8563 @ 0x51`
- A real INA226 connected at `0x40` was detected with `i2cdetect -y 0`, showing `40` and `51/UU`.
- The existing Linux kernel already contains `ina2xx.ko` with explicit `ina226` support.
- Runtime instantiation:
  `echo ina226 0x40 > /sys/bus/i2c/devices/i2c-0/new_device`
  successfully bound the device as `/sys/class/hwmon/hwmon1/name = ina226`.
- The driver reported `power monitor ina226 (Rshunt = 10000 uOhm)`.
- With `VBUS` connected to approximately 5 V and `IN+ / IN-` not used for a current path, Linux reported `in1_input = 4955 mV` while current and power remained `0`, consistent with the test wiring.
- This verifies a practical architecture in which **INA226 can be managed entirely by A35/Linux on I2C3**, avoiding M33-side access to I2C3.
- The runtime `new_device` method is temporary; production integration still requires a proper Linux device-tree node.

### FreeRTOS baseline

- `FreeRTOS_ThreadCreation` was built and run successfully in its own board-side project directory.
- The repeating output demonstrated task creation, scheduler operation, `osDelay()`, task suspend/resume, and repeated execution of Thread One and Thread Two.
- This is now a verified FreeRTOS reference/baseline for the ROV firmware.

### Current ROV peripheral direction

The current practical architecture is now:

```text
A35 / Linux
  I2C3
    ├── PCF8563 RTC
    └── INA226 power monitor

M33 / FreeRTOS
  I2C8
    ├── MPU6500 IMU
    └── LU9685 servo driver
  GPIO
    └── DHT11
  UART
    └── distance sensor
  OpenAMP / RPMsg
    └── A35 <-> M33 control/data path
```

- The I2C7 dead end is no longer blocking the project.
- The main I2C uncertainty has been resolved by **splitting responsibilities by execution environment rather than forcing every device onto M33**.
- Moving the low-rate INA226 monitoring to A35/Linux while reserving I2C8 for M33 application devices provides a cleaner and lower-risk architecture.

### Suggested next milestones

1. Keep I2C8 as the primary M33 sensor/actuator bus and consolidate the MPU6500 + LU9685 drivers under a shared FreeRTOS I2C mutex.
2. Add INA226 properly to the Linux I2C3 device tree and expose its readings through the existing `ina2xx`/hwmon path.
3. Finish the DHT11 hardware validation with the new bare sensor once available.
4. Validate the distance module independently, then integrate its UART path into M33.


---

# 2026-08-28 Progress Update

## Formal ROV_M33 project created and normalized

A dedicated formal project directory was established:

```text
C:\Users\64135\Desktop\ROV_M33
```

The project was created by evolving the already-verified `OpenAMP_TTY_echo_iic_LU9685` workflow rather than recreating the STM32MP257 CM33/OpenAMP build infrastructure from scratch.

The project is now formally named:

```text
Project:        ROV_M33
CPU project:    ROV_M33_CM33
NonSecure:      ROV_M33_CM33_NonSecure
ELF:            ROV_M33_CM33_NonSecure.elf
signed BIN:     ROV_M33_CM33_NonSecure_sign.bin
```

The CubeIDE build configuration remains:

```text
CA35TDCID_m33_ns_sign
```

This configuration name was deliberately retained because it is part of the already verified signing/build flow.

The project was clean-built with STM32CubeIDE 1.17.0:

```text
Build Finished. 0 errors, 0 warnings.
text  = 36764
data  = 360
bss   = 4296
total = 41420 bytes
```

The project-local `libmetal` copy remains in use so its CMake pre-build step does not write generated files into the read-only SDK tree.

No source code or files outside `ROV_M33` were modified during the naming cleanup.

## ROV_M33 board-side deployment workflow

The board uses Linux-managed remoteproc with:

```text
/sys/class/remoteproc/remoteproc0/fw_format = ELF
```

A dedicated deployment directory is used:

```text
/home/root/ROV_M33/
├── lib/
│   ├── fw_cortex_m33.sh
│   └── firmware/
│       └── ROV_M33_CM33_NonSecure.elf
```

A new project-specific `fw_cortex_m33.sh` was written rather than copying the old example script.

Its workflow is:

```text
ROV_M33/lib/firmware/ROV_M33_CM33_NonSecure.elf
    ↓
/lib/firmware/
    ↓
remoteproc0/firmware
    ↓
remoteproc0/state = start
```

The firmware was successfully started:

```text
remoteproc0/state    = running
remoteproc0/firmware = ROV_M33_CM33_NonSecure.elf
remoteproc0/fw_format = ELF
```

Kernel log confirmed:

```text
Booting fw image ROV_M33_CM33_NonSecure.elf
virtio_rpmsg_bus: rpmsg host is online
creating channel rpmsg-tty
remote processor m33 is now up
```

Therefore the renamed formal ROV firmware and project-specific deployment script are both **VERIFIED**.

## Formal OpenAMP/RPMsg command path

After starting the new ROV firmware, Linux again created:

```text
/dev/ttyRPMSG0
```

The command parser was verified from the Linux command line.

An undefined command:

```bash
echo "ask" > /dev/ttyRPMSG0
```

returned:

```text
err bad_cmd
```

This was taken as positive evidence that the new M33 firmware was receiving and parsing RPMsg traffic rather than a communication failure.

The formal servo command:

```bash
echo "set servo 0 90" > /dev/ttyRPMSG0
```

returned:

```text
ok servo 0 90
```

Therefore the following software path is **VERIFIED**:

```text
A35/Linux
  ↓
/dev/ttyRPMSG0
  ↓
OpenAMP/RPMsg
  ↓
M33 command parser
  ↓
servo API
  ↓
response via RPMsg
  ↓
A35/Linux
```

## LU9685 servo phase completed

The LU9685 servo implementation in `ROV_M33` was reviewed and incrementally corrected before deployment.

Verified software design:

```text
LU9685 driver
    ↓
20-channel state layer
    ↓
servo service
    ↓
RPMsg command interface
```

LU9685 protocol used:

```text
I2C address: 0x00
reset:       FB FB
single:      [channel, angle]
all:         [FD, angle0 ... angle19]
```

Servo channel resource mapping:

```text
CH0  = left_shoulder
CH1  = left_upper
CH2  = left_elbow
CH3  = left_front
CH4  = left_claw

CH5  = right_shoulder
CH6  = right_upper
CH7  = right_elbow
CH8  = right_front
CH9  = right_claw
```

Reserved future propulsion channels:

```text
CH10 = left_front_vertical
CH11 = left_back_vertical
CH12 = right_front_vertical
CH13 = right_back_vertical
CH14 = left_horizontal
CH15 = right_horizontal
```

Current remaining LU9685 channels:

```text
CH16~CH19 = reserved / unused by the ROV application
```

Servo API restrictions:

```text
CH0~CH9   → allowed by servo API
CH10~CH19 → rejected by servo API
```

The 20-channel state is maintained separately. `servo_set_all()` modifies only CH0~CH9 and submits a full 20-byte `0xFD` frame, preserving the current state of CH10~CH19.

At current initialization:

```text
CH0~CH9   = 90°
CH10~CH19 = 0xFF (PWM disabled according to the LU9685 protocol)
```

`servo_get()` is explicitly defined as returning the last successfully submitted command/target angle. The LU9685 provides no mechanical position feedback through this interface.

The LU9685 reset is executed during initialization before the first channel submission.

## LU9685 servo hardware acceptance

The new formal `ROV_M33_CM33_NonSecure.elf` was deployed and executed on the real STM32MP257D-ATK board.

Hardware verification completed:

```text
CH0 → servo motion verified
CH3 → servo motion verified
set servo all → verified
```

The verified end-to-end actuator chain is therefore:

```text
A35/Linux command
  ↓
/dev/ttyRPMSG0
  ↓
RPMsg/OpenAMP
  ↓
M33
  ↓
servo service
  ↓
I2C4
  ↓
LU9685 @ 0x00
  ↓
selected channel
  ↓
physical servo
```

This phase is now classified as:

```text
PHASE 1 — LU9685 SERVO
Implementation: VERIFIED
Build:          VERIFIED
RPMsg path:     VERIFIED
Hardware:       VERIFIED
```

The following are intentionally not claimed as individually hardware-tested:

```text
CH1, CH2, CH4, CH5, CH6, CH7, CH8, CH9
```

Their software support is implemented; only selected channels have been physically exercised.

## Current project architecture

The current development strategy is intentionally incremental:

```text
Phase 1
ROV_M33 project baseline
+
OpenAMP/RPMsg
+
I2C4
+
LU9685 CH0~CH9 servo
            ↓
        VERIFIED

Phase 2
I2C8
+
MPU6500
            ↓
        NEXT

Phase 3
CH10~CH15 ESC / propulsion
            ↓
        WAITING FOR
        final wiring definition

Phase 4
propeller mixer

Phase 5
attitude control / PID or PD

Phase 6
DYP distance sensor

Phase 7
DHT11

Phase 8
integrated ROV safety/control logic
```

The ESC/propeller phase is deliberately postponed because the final physical wiring, motor direction, and installation mapping still require additional hardware investigation. This does not block MPU6500 software integration.

## Architectural decisions retained

- I2C7 remains excluded from the ROV application because it is a Secure/CID1 PMIC bus controlled by the A35 Secure/OP-TEE side.
- I2C3 remains assigned to A35/Linux for low-rate devices such as INA226 rather than being accessed from M33.
- I2C8 remains the primary M33 application bus.
- I2C4 remains the current LU9685 actuator bus.
- UART4/COM11 is not required to prove M33 execution.
- RPMsg/remoteproc is the formal A35↔M33 control/data path.
- The project continues to use verified vendor examples as read-only reference material.
- The formal ROV project directory is the only intended writable development tree for Codex.

## Immediate next milestone

The next development phase is:

```text
ROV_M33
+
I2C8
+
MPU6500
```

The intended first step is to migrate the already-verified `OpenAMP_TTY_echo_iic_8` / MPU6500 workflow into the formal ROV project without yet implementing attitude estimation, PID, propulsion mixing, or ESC control.

The next phase should again follow:

```text
read/verify reference
→ implement one increment
→ clean build
→ code review
→ board test
→ freeze verified baseline
```

---

# 2026-08-28 Phase 2 / Step 2 Update

## I2C8 + MPU6500 software integration

Status:

```text
Implementation: IMPLEMENTED
Clean build:    PASSED (0 errors, 0 compiler warnings)
Hardware:       NOT VERIFIED
```

The formal `ROV_M33` project now contains an incremental I2C8 sensor path:

```text
A35/Linux
  ↓
/dev/ttyRPMSG0
  ↓
OpenAMP/RPMsg
  ↓
M33 command parser
  ↓
SensorService
  ↓
MPU6500 driver
  ↓
I2C8
```

Implemented I2C8 configuration:

```text
Handle:          I2c8Handle (independent from the I2C4 I2cHandle)
Peripheral:      I2C8
Timing:          0x2050606F
SCL:             PZ4 / AF8
SDA:             PZ9 / AF8
GPIO mode:       alternate-function open-drain, no pull, low speed
RIFSC resource:  48
GPIO resources:  GPIOZ4, GPIOZ9
RCC resource:    101
Transfer mode:   blocking HAL_I2C_Mem_Read/HAL_I2C_Mem_Write
DMA/IRQ:         none for I2C8
```

RCC resource 101 failure remains observable and non-blocking. I2C8 initialization continues after a failed RCC 101 request, matching the previously verified reference workflow. Shutdown releases RCC 101 only when this firmware actually acquired it.

The MPU6500 driver uses:

```text
7-bit address:   0x68
HAL address:     0x68 << 1
WHO_AM_I:        register 0x75, expected 0x70
PWR_MGMT_1:      0x00, followed by 100 ms delay
SMPLRT_DIV:      0x07
CONFIG:          0x00
GYRO_CONFIG:     0x00
ACCEL_CONFIG:    0x00
Accel raw:       registers 0x3B..0x40
Gyro raw:        registers 0x43..0x48
Byte order:      high byte first, combined into int16_t
```

The formal command added in this step is:

```text
sensor mpu
<seq> sensor mpu
```

Successful response format:

```text
ok <ax> <ay> <az> <gx> <gy> <gz>
<seq> ok <ax> <ay> <az> <gx> <gy> <gz>
```

The command returns raw MPU sensor-frame values only. No mounting transform, ROV body-frame conversion, attitude estimation, filtering, PID, or FreeRTOS task restructuring was added.

The MPU6500 is allowed to be absent at firmware startup. A failed sensor probe is logged but does not prevent the verified LU9685/OpenAMP baseline from starting; `sensor mpu` then returns the protocol-defined `err not_ready` response.

The clean build used STM32CubeIDE 1.17.0 with `CA35TDCID_m33_ns_sign` and generated:

```text
ROV_M33_CM33_NonSecure.elf
ROV_M33_CM33_NonSecure_sign.bin
```

`mpu6500.o` and `sensor_service.o` are present in the generated objects list. ELF symbols include `MPU6500_*` and `SensorService_*` while retaining `LU9685_*`, `servo_*`, `MX_OPENAMP_Init`, and `OPENAMP_check_for_message`.

No board or MPU6500 was connected for this step. WHO_AM_I, raw readings, RPMsg behavior, and I2C4/I2C8 coexistence still require later hardware validation.
