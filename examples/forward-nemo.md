# Forward NEMO

This deterministic example studies transport from configured releases using a NEMO product whose U and V variables share a horizontal grid. Provide `time_counter`, `deptht`, `nav_lon`, `nav_lat`, `uo`, and `vo`; `zos`, `tmask`, `wo`, and `avt` are optional under the documented fallback policy.

Build as described in `docs/build.md`, replace paths, and run `./build/wacommplusplus examples/forward-nemo.json`. Validate timestamps, dimensions, particle counts, and a constant-velocity displacement. Unsupported staggered U/V layouts fail. Missing W/AKT removes resolved vertical velocity/diffusion. Archive the configuration, revision, input/output checksums, seed, toolchain, platform, backend settings, tests, and tolerances. See `docs/adapters.md` and `tests/StructuredGridAdapterTest.cpp`.
