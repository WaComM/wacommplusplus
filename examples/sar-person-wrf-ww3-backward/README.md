# Coupled WRF–WW3 backward person-in-water drift

## Scientific objective

This scenario reconstructs a deterministic prior trajectory of a person in water from a later observed position under coincident ocean current, spatially varying 10 m wind, and surface Stokes drift.

## Prerequisites, configuration, and command

Provide `forcing.nc`, `wrf.nc`, `ww3.nc`, and `sources.json`, with the source time interpreted as the terminal observation. WRF requires `U10`, `V10`, `XLONG`, `XLAT`, `COSALPHA`, `SINALPHA`, and absolute time; WW3 requires time, coordinates, and eastward/northward surface Stokes velocity. Wind and Stokes components must declare meters per second, coordinates must declare degrees east/north, and numeric time must declare supported CF units relative to a UTC reference instant. This example opts into inverse-bilinear curvilinear geographic regridding for the WRF grid and rectilinear `bilinear_geographic` regridding for WW3; both source domains must enclose every ocean point and time axes must coincide after normalization. Build for CPU with `cmake -S . -B build && cmake --build build`, or for NVIDIA CUDA with `cmake -S . -B build-cuda -DUSE_CUDA=ON && cmake --build build-cuda`. Run the matching executable with `examples/sar-person-wrf-ww3-backward/sar-person-wrf-ww3-backward.json`.

## Expected behavior and validation

The solver traverses the chronological forcing in reverse while the WRF and WW3 adapters preserve their native temporal ordering and velocity signs. CUDA samples the same normalized brackets and applies direction only to the complete deterministic displacement. The deterministic velocity is ocean current plus empirical leeway plus WW3 surface Stokes drift. Validate with a matched forward run: in constant, boundary-free fields, the backward trajectory must recover its initial forward position within the declared integration tolerance. On a CUDA host, also compare complete particle state and output checksums with the serial backward run at a declared tolerance.

## Limitations, interpretation, and reproducibility

Deterministic backtracking is a kinematic reconstruction conditional on forcing and object coefficients, not a posterior probability distribution. WRF wind and WW3 Stokes components are bilinearly interpolated and therefore smoothed but not conservatively remapped; record source and target grids and quantify resolution sensitivity. Curvilinear cell lookup is spatially indexed, but highly overlapping cell bounds can still increase preprocessing cost. No projected-coordinate transform, coefficient uncertainty, jibing, or depth-dependent Stokes profile is applied. Classical leeway coefficients may already contain wave-correlated motion, so explicit Stokes addition requires observational calibration to avoid double counting. CUDA accelerates common-grid sampling and integration, not adapter normalization. Archive all ocean, WRF, and WW3 checksums, resolved configuration, Git revision, compiler and dependencies, CMake options, parallel layout, CUDA toolkit/driver/GPU where applicable, tolerances, and output checksums.

## References

- Breivik, Ø., Allen, A. A., Maisondieu, C., and Roth, J.-C. (2011). Wind-induced drift of objects at sea: the leeway field method. *Applied Ocean Research*, 33, 100–109. [doi:10.1016/j.apor.2011.01.005](https://doi.org/10.1016/j.apor.2011.01.005).
- Ardhuin, F., et al. (2010). Semiempirical dissipation source functions for ocean waves. Part I: definition, calibration, and validation. *Journal of Physical Oceanography*, 40, 1917–1941. [doi:10.1175/2010JPO4324.1](https://doi.org/10.1175/2010JPO4324.1).
- Jones, P. W. (1999). First- and second-order conservative remapping schemes for grids in spherical coordinates. *Monthly Weather Review*, 127, 2204–2210. [doi:10.1175/1520-0493(1999)127%3C2204:FASOCR%3E2.0.CO;2](https://doi.org/10.1175/1520-0493%281999%29127%3C2204%3AFASOCR%3E2.0.CO%3B2).
