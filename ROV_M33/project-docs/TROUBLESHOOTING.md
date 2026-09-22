# Troubleshooting

## Commands return `err safety` after rov_self_test

This is expected after a successful self-test. `rov_self_test` deliberately finishes with the global stop latch set and does not restore the prior motion state. Before intentional vehicle motion, the owning upper-layer controller must explicitly call `move()` after completing its own safety checks. Do not change the self-test to auto-move during cleanup.

## rov_self_test reports RPMsg timeout or inconsistent SEQ responses

Give RovControl exclusive ownership of `/dev/ttyRPMSG0`. Stop every concurrent `cat`, shell `echo`, terminal monitor, smoke tool, or second API process. A tty response is not broadcast, and another reader can steal it. For DYP failures, also verify that both UART4/CH342F Channel 1 jumpers on J4 are removed before attributing the failure to software.

## RovControl request times out while M33 and RPMsg remain healthy

Symptom: a RovControl call times out even though `remoteproc0` remains `running`, `/dev/ttyRPMSG0` exists, and the same API works once a second SSH command is stopped.

Confirmed cause: another process was concurrently running `cat /dev/ttyRPMSG0`. RPMsg tty responses are not broadcast; the competing reader can consume the SEQ response intended for RovControl. Concurrent `echo "..." > /dev/ttyRPMSG0` is also invalid because it bypasses the client's SEQ and request ownership.

Fix: give RovControl exclusive read/write ownership of `/dev/ttyRPMSG0`. Stop all shell `cat`, `echo`, terminal monitors, or other clients using that endpoint before starting the API process. This observed timeout is not evidence of a `RpmsgClient` parser, pending-map, or reader-thread defect.

## pyelftools missing
Symptom: `ModuleNotFoundError: No module named 'elftools'`.
Fix: install `pyelftools` in the Python environment used by the CubeIDE post-build step.

## Cryptodome missing
Symptom: `ModuleNotFoundError: No module named 'Cryptodome'`.
Fix: install `pycryptodomex`. The signing script also imports `cryptography`.

## remoteproc: rproc-m33-fw missing
Symptom: `Direct firmware load for rproc-m33-fw failed with error -2`.
Cause: M33 device-tree node has no `firmware-name`, so remoteproc generates the fallback name `rproc-m33-fw`.
Current fix: deploy the actual ELF and set `remoteproc0/firmware` to that filename.

## Vendor auto-loader rejects board
Symptom: vendor M33 firmware-load service runs but does not boot M33.
Cause: board ID `stm32mp257d-atk` is rejected by the vendor loader whitelist.
Evidence: `Board is not a valid BOARD`.
Current handling: bypass automatic loader with manual ELF deployment. No persistent BSP change.

## no resource table
Symptom: remoteproc prints `no resource table found for this firmware`.
Current meaning: expected for the non-OpenAMP base template. Revisit when OpenAMP/RPMsg is introduced.

## DK LED is not reliable on ATK
The DK template uses LED3=PH6, but ATK LED routing differs and PH6 is used by Linux. Use UART or another verified M33-controlled signal as execution evidence.
## Propeller single command returns `err io`

The pre-2026-08-30 `PropellerService_SetVertical/SetHorizontal` path called a full-state apply routine. A one-channel user command therefore recalculated CH10-CH15 and issued a contiguous six-channel/24-byte PWM transaction. This was unnecessary and made the observed `vertical 10 40 -> err io` indistinguishable from failures elsewhere in the larger transfer.

The current path submits one channel for an individual command. It still returns `err io` if that PCA9685/I2C transaction actually fails; the error is not swallowed. Inspect `ActuatorService_GetLastHalError()` for the retained `HAL_I2C_GetError()` value. A future board regression is still required to determine the concrete HAL cause of the historical observation.

## DYP `sensor dyp` repeatedly returns `err timeout`

Symptom: DYP initializes as ready, RPMsg and the MPU/control paths remain normal, but real-sensor requests time out. A fake DYP can still inject `FF 01 10 10` and obtain 272 mm.

Confirmed board cause: the UART4 jumpers on J4 connect CH342F Channel 1 to the same network used by DYP. CH342F TXD1 shares UART4_RX/PB6, and CH342F RXD1 shares the UART4_TX/PB7 net now used as the DYP trigger. This connection electrically interfered with real DYP communication.

Fix: power down as appropriate and remove both UART4/CH342F Channel 1 jumpers on J4. With CH342F physically isolated and no firmware, DYP wiring, level-converter, or UART setting change, M33 successfully received a real DYP response and reported 65533 mm. The value proves a complete UART response only; it is not documented as a valid distance.

### DYP/UART4 hardware preflight checklist

- Remove both J4 jumpers that connect UART4 to CH342F Channel 1.
- Confirm CH342F TXD1 is isolated from PB6/UART4_RX.
- Confirm CH342F RXD1 is isolated from the PB7 trigger net.
- Confirm PB6/AF3 is connected only to the level-shifted DYP TX path.
- Confirm PB7 GPIO trigger is connected only to the level-shifted DYP RX path.
- Confirm common ground, DYP 5 V supply, and correct level-converter directions before interpreting a timeout as a firmware failure.

## M33 goes completely silent with `E/TC: ... stm32_serc_handle_ilac ... SERC exception ID: 44`

Symptom: from one command onward the M33 answers nothing (`origin=client error=timeout` on every request) while `remoteproc0` stays `running`. The serial console shows an OP-TEE `E/TC` SERC illegal-access trace at the moment it wedged, and a later `echo stop > remoteproc0/state` logs `remote FW shutdown without ack`.

Confirmed cause (2026-09-15 incident): `SERC exception ID 44` = **I2C4** (PCA9685 bus). The M33 accessed I2C4 registers while the peripheral was unclocked/not provisioned for the core (secure-side clock/resource reconciliation can gate a clock the firmware had enabled with a bare RCC register write). SERD simultaneously returns a bus error to the M33; `BusFault_Handler` was a bare `while (1)` at IPCC interrupt priority, so SysTick, the main loop, and RPMsg all died permanently. Three wedges were captured in one boot window; the failure is intermittent and concentrated shortly after Linux boot.

Decoding SERC IDs: the ID is the hardware-block number from the SERF mapping tables (same numbering as the STM32MP25 unified RIF resource IDs, e.g. 32=USART2, 34=UART4, 44=I2C4; see `res_mgr_stm32mp25xxxx.h` and `dt-bindings/rif/stm32mp25-rifsc.h`). Access-under-reset/not-clocked/not-provisioned all raise SERC.

Fix shipped 2026-09-15:

- RPMsg command dispatch moved out of the IPCC RX interrupt into a main-loop queue (`CommandQueue` in `Core/Src/main.c`). Command handlers may still block on actuator I2C, but with SysTick running their HAL timeouts are real: a failing bus now degrades to `err io` replies instead of a locked core.
- I2C4/I2C8 RCC clock resources are now requested through `ResMgr_Request(RESMGR_RESOURCE_RIF_RCC, RESMGR_RCC_RESOURCE(<peripheral id>))` as best-effort hardening. A denial is deliberately non-fatal (banner `M33: I2C4 clock resource DENIED; continuing`); the first fatal attempt bricked boot because the request is denied on this board.
- Remaining gap (tracked as future work): fault handlers are still `while (1)` loops. A future change should record the fault and recover (M33 self-reset or IWDG via the registered `stm32-rproc` wdg irq); verify the reset-scope semantics of `NVIC_SystemReset()` for the MP257 M33 before using it, because a core-local reset is required.

## Gateway DYP polling fails while `rov_self_test` DYP passes

Symptom: `rov_gateway` journal repeats `sensor: dyp failure xN: rov error 5: M33 returned an error` and the 0x0100 summary shows `dypBit=0`, while a manual `rov_self_test` on an idle M33 shows `DYP communication PASS`.

Confirmed cause: `DYP_RESPONSE_TIMEOUT_MS` was 60 ms. The module answers after the trigger pulse rising edge, and the M33 main loop releases the trigger with jitter; under the gateway's 100 Hz MPU polling the reply regularly arrived after the 60 ms window, so the M33 itself returned `err timeout`. On an idle core the same window was just sufficient.

Fix (2026-09-15): `DYP_RESPONSE_TIMEOUT_MS` 60 -> 200 ms. Verified: gateway DYP polling error-free over sustained 100 Hz load; `65533`-class frames are the documented bench condition (OutOfRange, D-10), not an error.

## M33 firmware builds after moving to a new PC

The CubeIDE project links SDK sources by absolute path and the post-build signer referenced a machine-local Python. On the current build host:

- SDK sources are staged at `E:\STM32MP257\SDK_STAGE\STM32Cube_ATK_FW_MP2_V1.1.0` (Drivers/Utilities from `STM32Cube_FW_MP2_V1.3.1`, OpenAMP from this repo's `Middlewares` copy which keeps the older libmetal layout the project links), and `.project`/`.cproject` point there.
- The staged `Utilities/CoproSync/copro_sync.c` uses the application handle name `hipcc` (stock ST uses `hipcc1`); keep the patch when re-staging.
- The post-build signing step uses `F:/Python/Python314/python.exe` with `PYTHONPATH=Tools/python` (vendored `elftools`/`Cryptodome`).
- Headless build: `stm32cubeidec.exe -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -data <ws> -build ROV_M33_CM33_NonSecure/CA35TDCID_m33_ns_sign`. The libmetal pre-build cmake needs `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` with CMake >= 4 (already in `.cproject`).
