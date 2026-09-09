# Coupled WRF–WW3 backward person-in-water drift

## Scientific objective

This scenario reconstructs a deterministic prior trajectory of a person in water from a later observed position under coincident ocean current, spatially varying 10 m wind, and surface Stokes drift.

## Prerequisites, configuration, and command

Provide `forcing.nc`, `wrf.nc`, `ww3.nc`, and `sources.json`, with the source time interpreted as the terminal observation. WRF requires `U10`, `V10`, `XLONG`, `XLAT`, `COSALPHA`, `SINALPHA`, and absolute time; WW3 requires time, coordinates, and eastward/northward surface Stokes velocity in m s-1. All normalized grids and timestamps must coincide. Build with `cmake -S . -B build && cmake --build build`, then run `./build/wacommplusplus examples/sar-person-wrf-ww3-backward.json`.

## Expected behavior and validation

The solver traverses the chronological forcing in reverse while the WRF and WW3 adapters preserve their native temporal ordering and velocity signs. The deterministic velocity is ocean current plus empirical leeway plus WW3 surface Stokes drift. Validate with a matched forward run: in constant, boundary-free fields, the backward trajectory must recover its initial forward position within the declared integration tolerance.

## Limitations, interpretation, and reproducibility

Deterministic backtracking is a kinematic reconstruction conditional on forcing and object coefficients, not a posterior probability distribution. No environmental regridding, coefficient uncertainty, jibing, or depth-dependent Stokes profile is applied. Classical leeway coefficients may already contain wave-correlated motion, so explicit Stokes addition requires observational calibration to avoid double counting. Archive all ocean, WRF, and WW3 checksums, resolved configuration, Git revision, compiler and dependencies, CMake options, parallel layout, tolerances, and output checksums. Dynamic WRF/WW3 coupling is not yet supported by CUDA execution.

## References

- Breivik, Ø., Allen, A. A., Maisondieu, C., and Roth, J.-C. (2011). Wind-induced drift of objects at sea: the leeway field method. *Applied Ocean Research*, 33, 100–109. [doi:10.1016/j.apor.2011.01.005](https://doi.org/10.1016/j.apor.2011.01.005).
- Ardhuin, F., et al. (2010). Semiempirical dissipation source functions for ocean waves. Part I: definition, calibration, and validation. *Journal of Physical Oceanography*, 40, 1917–1941. [doi:10.1175/2010JPO4324.1](https://doi.org/10.1175/2010JPO4324.1).
