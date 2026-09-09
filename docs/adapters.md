# Ocean adapters

Adapters normalize ocean time, vertical levels, mask, longitude, latitude, bathymetry, sea-surface height, velocity, and diffusivity onto the particle grid. They never reverse time or velocity. The solver owns tracking direction.

The implemented adapters are ROMS, native WACOMM, NEMO, and HYCOM. Adapter selection is explicit and an unknown name fails; no fallback chooses a different model silently.

ROMS velocity is normalized from its Arakawa C-grid staggering to the rho-point particle grid without changing units or sign. For every wet rho point, `U_rho(j,i)` is the arithmetic mean of the valid wet faces `U(j,i)` and `U(j,i-1)`, while `V_rho(j,i)` is the mean of `V(j,i)` and `V(j-1,i)`. A domain edge with one available face uses that face rather than halving it against an invented zero; a land rho point is zero. Thus velocity remains in meters per second and the adapter performs spatial normalization only. The runtime ROMS fixture checks interior and all four edge mappings on the staggered dimensions described by Shchepetkin and McWilliams (2005).

Native WACOMM files use the `xi_rho` horizontal dimension written by the current serializer. The loader also accepts the historical `eta_xi` spelling for restart/input compatibility. Native and ROMS time coordinates must be strictly chronological, matching the NEMO/HYCOM contract.

NEMO recognizes `nav_lon/longitude/lon`, `nav_lat/latitude/lat`, `time_counter/time`, `uo/vozocrtx/u`, `vo/vomecrty/v`, `wo/vovecrtz/w`, `zos/sossheig/ssh`, `avt/votkeavt/akt`, and `deptht/depth`. HYCOM recognizes `lon/longitude`, `lat/latitude`, `time/MT`, `water_u/u`, `water_v/v`, `surf_el/ssh/zeta`, `water_w/w`, `diffusivity/akt`, and `bathymetry`. HYCOM longitudes greater than 180 degrees are normalized by subtracting 360 degrees.

The initial NEMO implementation accepts products whose U and V fields already share the particle grid. It detects and rejects unsupported staggered or inconsistent dimensions. Input time must be strictly chronological. Positive-down depth may be stored shallow-to-deep or deep-to-shallow, but must be strictly monotonic; the adapter maps depth and every dynamic vertical field together onto WaComM's bottom-to-surface logical indices. Missing W or AKT fields use zero with a warning; this excludes resolved vertical transport or turbulent vertical diffusion and must be considered in scientific interpretation.

Run `ctest --test-dir build -R "roms_adapter|native_adapter|structured_grid_adapters" --output-on-failure` after configuring an application build. The fixtures are generated at runtime and require NetCDF C++4. Failures normally indicate an unsupported dimension layout, a non-chronological time axis, or a mismatch between coordinates and velocity dimensions. Adapter fixture success verifies normalization, not the scientific suitability or resolution of a forcing product.
