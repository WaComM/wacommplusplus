# Backward SAR drift with projected wave forcing

## Scientific objective

Scientific question: which prior location follows when current, leeway, and projected-grid Stokes velocity are integrated backward from a person-in-water endpoint?

## Prerequisites, configuration, and command

Provide endpoint particles in `sources.json`, chronological ROMS `forcing.nc`, and `ww3-epsg3857.nc` with `time`, rectilinear `x/y` in meters, and Earth-relative eastward/northward Stokes velocity in m s-1. Confirm independently that the declared `EPSG:3857` describes the file. Build with `cmake -S . -B build -DUSE_PROJ=ON && cmake --build build`, then execute `./build/wacommplusplus examples/sar-person-projected-wave-backward.json`.

## Expected behavior and validation

Adapters preserve chronological forcing and vector signs. PROJ maps ocean-grid EPSG:4326 positions into the declared source CRS; the solver alone applies negative time orientation to current plus leeway plus Stokes velocity. A matched constant-field forward/backward pair must recover its initial point within the declared tolerance, and projected and geographic representations of the same analytical field must agree after transformation.

## Limitations, interpretation, and reproducibility

This is a conditional kinematic reconstruction, not a unique origin or posterior distribution. Bilinear interpolation is non-conservative, no extrapolation is allowed, and the transformation does not rotate projected grid-relative vectors. EPSG:3857 is materially distorted at high latitude. Archive the CRS text, PROJ version/database, Git revision, resolved configuration, forcing/source/output checksums, seed, toolchain, CMake options, backend layout, and numerical tolerances.

## References

- Thygesen, U. H. (2011). How to reverse time in stochastic particle tracking models. *Journal of Marine Systems*, 88, 159–168. [doi:10.1016/j.jmarsys.2011.03.009](https://doi.org/10.1016/j.jmarsys.2011.03.009).
- Breivik, Ø., Allen, A. A., Maisondieu, C., and Roth, J.-C. (2011). Wind-induced drift of objects at sea: the leeway field method. *Applied Ocean Research*, 33, 100–109. [doi:10.1016/j.apor.2011.01.005](https://doi.org/10.1016/j.apor.2011.01.005).
