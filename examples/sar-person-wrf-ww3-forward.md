# Coupled WRF–WW3 person-in-water drift

## Scientific objective

This scenario estimates deterministic forward displacement of a person in water under coincident ocean current, spatially varying 10 m wind, and surface Stokes drift.

## Prerequisites, configuration, and command

Provide `forcing.nc`, `wrf.nc`, `ww3.nc`, and `sources.json`. WRF requires `U10`, `V10`, `XLONG`, `XLAT`, `COSALPHA`, `SINALPHA`, and absolute time; WW3 requires time, coordinates, and eastward/northward surface Stokes velocity in m s-1. All normalized grids and timestamps must coincide. Build with `cmake -S . -B build && cmake --build build`, then run `./build/wacommplusplus examples/sar-person-wrf-ww3-forward.json`.

## Expected behavior and validation

The deterministic velocity is ocean current plus empirical leeway plus WW3 surface Stokes drift. Validate each adapter against known component values, then compare the coupled constant-field displacement with the analytical velocity sum. A forward/backward pair must recover the initial position within the declared integration tolerance when stochastic terms and boundary interactions are absent.

## Limitations, interpretation, and reproducibility

No environmental regridding, coefficient uncertainty, jibing, or depth-dependent Stokes profile is applied. The result is conditional on object class and forcing accuracy and is not an operational search area. Archive all input checksums, resolved configuration, Git revision, compiler and dependencies, CMake options, parallel layout, tolerances, and output checksums. Dynamic WRF/WW3 coupling is not yet supported by CUDA execution.

## References

- Breivik, Ø., Allen, A. A., Maisondieu, C., and Roth, J.-C. (2011). Wind-induced drift of objects at sea: the leeway field method. *Applied Ocean Research*, 33, 100–109. [doi:10.1016/j.apor.2011.01.005](https://doi.org/10.1016/j.apor.2011.01.005).
- Ardhuin, F., et al. (2010). Semiempirical dissipation source functions for ocean waves. Part I: definition, calibration, and validation. *Journal of Physical Oceanography*, 40, 1917–1941. [doi:10.1175/2010JPO4324.1](https://doi.org/10.1175/2010JPO4324.1).
