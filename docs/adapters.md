# Ocean adapters

![Ocean, weather, and wave adapters normalize product data before a single solver composes the physics](figures/environment-adapter-schema.svg)

Adapters normalize ocean time, vertical levels, mask, longitude, latitude, bathymetry, sea-surface height, velocity, and diffusivity onto the particle grid. They never reverse time or velocity. The solver owns tracking direction.

The implemented adapters are ROMS, native WACOMM, NEMO, and HYCOM. Adapter selection is explicit and an unknown name fails; no fallback chooses a different model silently.

ROMS velocity is normalized from its Arakawa C-grid staggering to the rho-point particle grid without changing units or sign. For every wet rho point, `U_rho(j,i)` is the arithmetic mean of the valid wet faces `U(j,i)` and `U(j,i-1)`, while `V_rho(j,i)` is the mean of `V(j,i)` and `V(j-1,i)`. A domain edge with one available face uses that face rather than halving it against an invented zero; a land rho point is zero. Thus velocity remains in meters per second and the adapter performs spatial normalization only. The runtime ROMS fixture checks interior and all four edge mappings on the staggered dimensions described by Shchepetkin and McWilliams (2005).

Native WACOMM files use the `xi_rho` horizontal dimension written by the current serializer. The loader also accepts the historical `eta_xi` spelling for restart/input compatibility. Native and ROMS time coordinates must be strictly chronological, matching the NEMO/HYCOM contract.

NEMO recognizes `nav_lon/longitude/lon`, `nav_lat/latitude/lat`, `time_counter/time`, `uo/vozocrtx/u`, `vo/vomecrty/v`, `wo/vovecrtz/w`, `zos/sossheig/ssh`, `avt/votkeavt/akt`, and `deptht/depth`. HYCOM recognizes `lon/longitude`, `lat/latitude`, `time/MT`, `water_u/u`, `water_v/v`, `surf_el/ssh/zeta`, `water_w/w`, `diffusivity/akt`, and `bathymetry`. HYCOM longitudes greater than 180 degrees are normalized by subtracting 360 degrees.

The initial NEMO implementation accepts products whose U and V fields already share the particle grid. It detects and rejects unsupported staggered or inconsistent dimensions. Input time must be strictly chronological. Positive-down depth may be stored shallow-to-deep or deep-to-shallow, but must be strictly monotonic; the adapter maps depth and every dynamic vertical field together onto WaComM's bottom-to-surface logical indices. Missing W or AKT fields use zero with a warning; this excludes resolved vertical transport or turbulent vertical diffusion and must be considered in scientific interpretation.

Run `ctest --test-dir build -R "roms_adapter|native_adapter|structured_grid_adapters" --output-on-failure` after configuring an application build. The fixtures are generated at runtime and require NetCDF C++4. Failures normally indicate an unsupported dimension layout, a non-chronological time axis, or a mismatch between coordinates and velocity dimensions. Adapter fixture success verifies normalization, not the scientific suitability or resolution of a forcing product.

Atmospheric and wave inputs are deliberately independent of the ocean adapter family. `WeatherModelAdapter` and `WeatherModelAdapterFactory` expose generic chronological time, longitude, latitude, and eastward/northward 10 m wind fields. The `WRFAdapter` reads `U10`, `V10`, `XLONG`, `XLAT`, `COSALPHA`, `SINALPHA`, and either absolute numeric `time` or `Times`. Native grid-relative wind is rotated to Earth-relative components using `u_e=U10 cos(alpha)-V10 sin(alpha)` and `v_n=V10 cos(alpha)+U10 sin(alpha)` before it reaches drift physics.

`WaveModelAdapter` and `WaveModelAdapterFactory` expose the corresponding generic surface Stokes velocity. The `WW3Adapter` accepts chronological `time`, one- or two-dimensional longitude/latitude, and the component aliases `uuss/vuss`, `ust/vst`, `stokes_u/stokes_v`, `eastward_surface_stokes_drift/northward_surface_stokes_drift`, or `sea_surface_wave_stokes_drift_x_velocity/sea_surface_wave_stokes_drift_y_velocity`, all in m s-1. The last two pairs improve interoperability with CF-oriented products and OpenDrift reader conventions without coupling WaComM++ to OpenDrift. Longitude above 180 degrees is normalized to the `[-180,180]` convention.

The present coupling requires each environmental forcing list to contain one file per ocean forcing window. After the same chronological boundary-record assembly used by ocean forcing, weather and wave times and coordinates must match the normalized ocean particle grid within declared numerical tolerances. The application fails if regridding would be required; it never treats unequal grids as coincident. ROMS, NEMO, HYCOM, native WACOMM, WRF, and WW3 adapters never reverse time or velocity. Direction remains solver policy.

Environmental metadata are part of the adapter contract. WRF and WW3 horizontal velocities must declare units equivalent to `m s-1`; longitude and latitude must declare `degrees_east` and `degrees_north`, respectively. Numeric environmental time must use a supported Gregorian calendar and CF-style units in seconds, minutes, hours, or days since a UTC reference date. Missing, non-finite, ambiguous, or unsupported metadata cause an error rather than an implicit scale conversion. WRF character `Times` remains supported because it carries an explicit `YYYY-MM-DD_HH:MM:SS` timestamp.

If `τ` is the numeric coordinate, `sU` is its unit scale in seconds, `t0` is the declared reference instant, and `tW` is the WaComM epoch, normalization is

$$
t_{\mathrm{WaComM}}=(t_0-t_W)+s_U\tau .
$$

This is representation normalization only. It does not reorder records or change tracking direction. The first implementation supports `standard`, `gregorian`, and `proleptic_gregorian` calendars for modern forcing dates and rejects non-Gregorian model calendars until their chronology is implemented explicitly.

## References

- Shchepetkin, A. F., and McWilliams, J. C. (2005). The regional oceanic modeling system (ROMS): a split-explicit, free-surface, topography-following-coordinate oceanic model. *Ocean Modelling*, 9, 347–404. [doi:10.1016/j.ocemod.2004.08.002](https://doi.org/10.1016/j.ocemod.2004.08.002).
- Bleck, R. (2002). An oceanic general circulation model framed in hybrid isopycnic-Cartesian coordinates. *Ocean Modelling*, 4, 55–88. [doi:10.1016/S1463-5003(01)00012-9](https://doi.org/10.1016/S1463-5003(01)00012-9).
- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Powers, J. G., et al. (2017). The Weather Research and Forecasting Model: overview, system efforts, and future directions. *Bulletin of the American Meteorological Society*, 98, 1717–1737. [doi:10.1175/BAMS-D-15-00308.1](https://doi.org/10.1175/BAMS-D-15-00308.1).
- Tolman, H. L. (1991). A third-generation model for wind waves on slowly varying, unsteady, and inhomogeneous depths and currents. *Journal of Physical Oceanography*, 21, 782–797. [doi:10.1175/1520-0485(1991)021%3C0782:ATGMFW%3E2.0.CO;2](https://doi.org/10.1175/1520-0485(1991)021%3C0782:ATGMFW%3E2.0.CO;2).
- Hassell, D., Gregory, J., Blower, J., Lawrence, B. N., and Taylor, K. E. (2017). A data model of the Climate and Forecast metadata conventions (CF-1.6) with a software implementation (cf-python v2.1). *Geoscientific Model Development*, 10, 4619–4646. [doi:10.5194/gmd-10-4619-2017](https://doi.org/10.5194/gmd-10-4619-2017).
