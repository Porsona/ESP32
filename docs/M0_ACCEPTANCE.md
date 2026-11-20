# M0 Stable — acceptance before M1

Date: 2026-11-19 (Asia/Saigon).

The user explicitly confirmed hardware acceptance before requesting M1:
- M0.0 Hardware Baseline PASS.
- M0.1 Display Diagnostics PASS.
- M0.2 calibration/NVS/events/CAL PASS.
- M0.3 Home/AppManager PASS, 8/8 user-run tests.

These are user-supplied results, not board tests performed by the agent.
The old detailed cases in TEST_REPORT_M0.md remain historical; no additional
heap values, 30-minute measurements, or individual results are invented here.

Stable firmware: M0_SHELL_01. SHA-256 BIN:
E224360C21C37314F4E0946A1BFF7CDCB3F0D2F9AC7DBB489ED1B925F0483C63.

No Git repository existed at audit time. Before source changes, a complete
source/config/BIN/ELF backup with SHA256.json was created at
docs/baselines/M0_STABLE_before_M1_2026-11-19/.
Local Git is initialized for the requested M0 commit/tag and M1 feature branch.
Backups and generated compiler/build artifacts remain outside versioned source.
No board NVS was read, cleared or rewritten by this backup operation.

The hardware, calibration schema, driver, GPIO/SPI, rotation 1, inversion true,
GLCD font and firmware dependencies must remain unchanged during M1.
