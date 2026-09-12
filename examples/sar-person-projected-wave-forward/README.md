# Forward SAR drift with projected wave forcing

## Scientific objective

Scientific question: how does an explicitly georeferenced projected WW3 Stokes field modify a forward person-in-water trajectory driven by ROMS current and uniform 10 m wind?

## Prerequisites, configuration, and command

Provide chronological `forcing.nc`, `sources.json`, and `ww3-epsg3857.nc`. The WW3 file must contain `time`, rectilinear one-dimensional `x` and `y` coordinates in meters, and eastward/northward Stokes components in m s-1 on `[time,y,x]`. Its coordinates must truly use EPSG:3857; the string is a scientific declaration, not an inference. Build with `cmake -S . -B build -DUSE_PROJ=ON && cmake --build build`, then run `./build/wacommplusplus examples/sar-person-projected-wave-forward/sar-person-projected-wave-forward.json`.

## Expected behavior and validation

PROJ transforms every ocean-grid longitude/latitude point from EPSG:4326 into EPSG:3857, after which the existing rectilinear bilinear operator samples the already Earth-relative Stokes components. Validate coordinate transformation against authoritative control points and compare a constant field exactly. An affine projected-coordinate fixture must match analytical bilinear values within the declared floating-point tolerance. A build without PROJ must reject the configuration.

## Limitations, interpretation, and reproducibility

Coordinate transformation does not rotate vector components or make interpolation conservative. The input components must already be eastward and northward; projected grid-relative vectors are unsupported. EPSG:3857 distorts scale and area and is used here as a verifiable example, not a recommendation for regional ocean products. No extrapolation occurs. This deterministic trajectory is conditional on forcing and object coefficients, not an operational search area. Archive CRS definition and PROJ version/database, Git revision, configuration, all checksums, seed, compiler/dependencies, CMake options, parallel layout, tolerances, and output checksums.

## References

- Karney, C. F. F. (2013). Algorithms for geodesics. *Journal of Geodesy*, 87, 43–55. [doi:10.1007/s00190-012-0578-z](https://doi.org/10.1007/s00190-012-0578-z).
- Breivik, Ø., Allen, A. A., Maisondieu, C., and Roth, J.-C. (2011). Wind-induced drift of objects at sea: the leeway field method. *Applied Ocean Research*, 33, 100–109. [doi:10.1016/j.apor.2011.01.005](https://doi.org/10.1016/j.apor.2011.01.005).
