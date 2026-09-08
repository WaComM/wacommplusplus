# Backward HYCOM

This example estimates deterministic candidate prior positions from endpoint particles under HYCOM forcing. It requires the forward HYCOM variables and a direction-compatible endpoint restart; sources remain disabled.

Replace paths and run `./build/wacommplusplus examples/backward-hycom.json`. Validate using a diffusion-free round trip away from closures. Missing W/AKT and forcing resolution limit interpretation, and output is not proof of a unique origin. Preserve the full reproducibility record and see `docs/backtracking.md`, `docs/adapters.md`, and the adapter fixture test.
