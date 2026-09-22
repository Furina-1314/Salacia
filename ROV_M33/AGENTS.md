# ROV_M33 Development — Codex Instructions

## Scope
This repository is the formal STM32MP257 Cortex-M33 ROV firmware project. Its writable project root is `C:\Users\64135\Desktop\ROV_M33`. Vendor SDKs, board packages, experiments, and verified example projects outside this directory are read-only references unless the user explicitly authorizes a narrowly scoped operation.

## Hardware
- Board: 正点原子 STM32MP257DAK3
- A35: Linux / upper-layer processing
- M33: real-time-control target
- Do not assume DK pin mappings equal ATK mappings. Verify actual hardware routing and Linux resource ownership.

## Software
- Host: Windows
- STM32CubeIDE: 1.17.0
- SDK/reference package: 正点原子 `STM32Cube_ATK_FW_MP2_V1.1.0` plus ST STM32CubeMP2 components
- Target: Linux on A35
- M33 deployment: Linux remoteproc
- Current remoteproc0 firmware format: ELF

## Formal M33 project
- CubeIDE project: `STM32CubeIDE/CM33/NonSecure`
- Project name: `ROV_M33_CM33_NonSecure`
- Build configuration: `CA35TDCID_m33_ns_sign`
- ELF: `ROV_M33_CM33_NonSecure.elf`
- Signed image: `ROV_M33_CM33_NonSecure_sign.bin`
- The local libmetal source at `Middlewares/Third_Party/OpenAMP/libmetal` must remain the pre-build source.

## Rules
1. Read `project-docs/PROJECT_STATE.md` before substantial work.
2. Read only documentation relevant to the current task.
3. Treat verified facts in PROJECT_STATE as current truth unless new evidence contradicts them.
4. Do not repeat already-verified investigations without a reason.
5. Make the smallest change needed for the current task.
6. Do not expand scope without approval.
7. Do not modify external SDKs, reference projects, device tree, systemd, vendor BSP, or persistent target configuration unless explicitly requested and justified.
8. Preserve known-good states before risky changes.
9. Validate one subsystem at a time.
10. Preserve the verified OpenAMP/RPMsg, I2C4, LU9685, signing, linker, and remoteproc workflows unless the current task explicitly changes them.
11. Build and test after code changes.
12. Stop after the requested milestone.
13. Update PROJECT_STATE only when a fact or milestone is actually verified.
14. Record durable decisions in DECISIONS.md.
15. Record reusable failures in TROUBLESHOOTING.md.
16. Keep this file short; no transient logs here.

## Formal A35/M33 control protocol
The formal A35/M33 control protocol is `project-docs/ROV_A35_M33_Control_Protocol_v1.0.md`.

For command names, command parameters, wire format, SEQ, return codes, `stop`, `safe`, `emergency`, `horizontal`, `servo`, `propeller`, `sensor`, and `event`, that document is authoritative. Do not define a second protocol from memory.

## Current baseline and sequence
The formal ROV_M33 baseline, OpenAMP/RPMsg path, I2C4, LU9685, and selected servo channels are verified. Continue one approved subsystem at a time from the milestones recorded in `project-docs/PROJECT_STATE.md`; do not automatically start a later phase.

## Completion report
Report: changed files, build/test result, verified facts, remaining uncertainty, and whether project state was updated.
