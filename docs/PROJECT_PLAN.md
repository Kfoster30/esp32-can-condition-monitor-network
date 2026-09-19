# Project Plan

## Phase 1 — Protocol foundation

- Create separate sensor and dashboard firmware builds.
- Define three versioned eight-byte messages.
- Add independent host-side codec tests and golden vectors.
- Document safe wiring and measurable acceptance criteria.

Exit criterion: both roles compile and all protocol tests pass.

## Phase 2 — Two-node bench network

- Inspect the SN65HVD230 modules for onboard termination.
- Assemble a short, correctly terminated twisted-pair bus.
- Flash the simulated sensor and dashboard roles.
- Capture healthy traffic, measure message timing, and verify zero sequence gaps.
- Disconnect/reconnect the bus to verify timeout and recovery behavior.

Exit criterion: repeatable two-node communication and fault recovery evidence.

## Phase 3 — Real condition data

- Reuse selected, proven sensor logic from Project 1 without coupling the repos.
- Replace simulated temperature, vibration, voltage, and current values.
- Track sensor validity in the CAN quality and status fields.
- Repeat the automated and bench validation suite.

Exit criterion: dashboard values match the sensor ECU and reference measurements.

## Phase 4 — J1939-style networking

- Formalize priority, PGN, destination, and source-address fields.
- Add address and message documentation.
- Add a diagnostic/status PGN and network fault tests.
- Clearly document which J1939 features are implemented and which are not.

Exit criterion: portfolio-ready release with traceable test evidence.

