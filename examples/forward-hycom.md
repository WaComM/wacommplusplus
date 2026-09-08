# Forward HYCOM

This deterministic example studies transport from releases under HYCOM/GOFS forcing. Provide longitude, latitude, positive-down depth, time/MT, and `water_u/water_v`; surface elevation, vertical velocity, diffusivity, mask, and bathymetry are optional under the adapter policy. Longitudes in 0–360 degrees are normalized.

Replace paths and run `./build/wacommplusplus examples/forward-hycom.json`. Validate normalized longitude, times, particle count, and constant-flow displacement. Zero W/AKT fallbacks exclude vertical transport/diffusion. Archive all metadata listed in `docs/reproducibility.md`; see `tests/StructuredGridAdapterTest.cpp`.
