# Backward NEMO

This example calculates deterministic candidate prior positions for endpoint particles under NEMO circulation. It has the same field and common-grid prerequisites as the forward NEMO example and requires a direction-compatible endpoint restart.

Replace paths and run `./build/wacommplusplus examples/backward-nemo.json`. Validate with a diffusion-free forward/backward round trip away from boundaries. Results are conditional candidate origins, not proof of a unique source. Record the complete reproducibility metadata and consult `docs/backtracking.md`, `docs/adapters.md`, and the structured-adapter fixture test.
