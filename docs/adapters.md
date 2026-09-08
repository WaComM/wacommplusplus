# Ocean adapters

Adapters normalize ocean time, vertical levels, mask, longitude, latitude, bathymetry, sea-surface height, velocity, and diffusivity onto the particle grid. They never reverse time or velocity. The solver owns tracking direction.

The implemented adapters are ROMS, native WACOMM, NEMO, and HYCOM. Adapter selection is explicit and an unknown name fails; no fallback chooses a different model silently.

NEMO recognizes `nav_lon/longitude/lon`, `nav_lat/latitude/lat`, `time_counter/time`, `uo/vozocrtx/u`, `vo/vomecrty/v`, `wo/vovecrtz/w`, `zos/sossheig/ssh`, `avt/votkeavt/akt`, and `deptht/depth`. HYCOM recognizes `lon/longitude`, `lat/latitude`, `time/MT`, `water_u/u`, `water_v/v`, `surf_el/ssh/zeta`, `water_w/w`, `diffusivity/akt`, and `bathymetry`. HYCOM longitudes greater than 180 degrees are normalized by subtracting 360 degrees.

The initial NEMO implementation accepts products whose U and V fields already share the particle grid. It detects and rejects unsupported staggered dimensions. Missing W or AKT fields use zero with a warning; this excludes resolved vertical transport or turbulent vertical diffusion and must be considered in scientific interpretation.
