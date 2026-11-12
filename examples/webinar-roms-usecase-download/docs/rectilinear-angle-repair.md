# Explicit rectilinear ROMS angle repair

## Scientific question and interpretation

Can the downloaded September 15–16, 2026 forcing be prepared under an explicit
ROMS grid-axis velocity interpretation despite contradictory angle metadata?
The paired JSON selects that interpretation as a preprocessing assumption. It
is not evidence that the upstream ocean simulation used the correct orientation.
This example prepares passive transport forcing; it selects no drift model.

## Prerequisites and required fields

Use Python with NumPy and netCDF4, the complete checksummed download mirror, and
`tools/repair_angle.py`. The input must declare a spherical grid, geographic
longitude/latitude in `degree_east`/`degree_north`, staggered ROMS U/V in
`meter second-1` at `edge1`/`edge2`, and rho-point `angle` in radians. Required
vertical coordinates, masks, bathymetry (m), elevation (m), vertical velocity
(m/s), diffusivity (m²/s), and physical time are copied for the ROMS adapter.
The adapter independently validates its supported forcing contract on conversion.

## Configuration and numerical operator

The JSON explicitly declares spherical geographic coordinates with radius
6,371,000 m, longitude/latitude axis order, and longitude domain [−180°, 180°].
No projection or datum transformation is performed. No CRS is inferred from a
product name, filename, or coordinate range.

For every rho point `(j,i)`, longitude must depend only on `i`, latitude only
on `j`, and both coordinates must increase along their respective axes. U/V
coordinates must equal the arithmetic means of adjacent rho coordinates along
XI/ETA, respectively. The maximum absolute coordinate residual is at most
10⁻¹⁰ degrees. All geometry must be finite and present; poles, cyclic longitude,
reversed axes, curved grids, packed velocities and explicit Earth-relative
velocity standard names are rejected. Vertical stretching curves must exactly
equal their corresponding sigma coordinates for this workflow.

The supported source angle is π/2 within 10⁻⁸ radians. Under the configured
XI/ETA component interpretation and the checked geometry, the repair sets
`angle = 0` radians. The downstream normalization is

`u_E = u_XI cos(angle) − v_ETA sin(angle)`

`v_N = u_XI sin(angle) + v_ETA cos(angle)`.

Here `u_E,v_N` are eastward/northward velocity, `u_XI,v_ETA` are the existing
wet-face averages, all in m/s; `angle` is XI orientation counterclockwise from
east, in radians. Thus the repair leaves the velocity components unchanged.
This operator does not interpolate, regrid, extrapolate, or claim conservation
properties beyond exact preservation of retained stored values. ROMS grid
formulation is described by Shchepetkin and McWilliams (2005).

## Exact command and expected outputs

From the repository root, using the scientific Python environment:

```sh
data/wacomm-sarno-lite/venv/bin/python examples/webinar-roms-usecase-download/tools/repair_angle.py \
  --source-root examples/webinar-roms-usecase-download/data/roms-20260915-16 \
  --output-root examples/webinar-roms-usecase-download/data/roms-20260915-16-angle-repaired-001 \
  --policy examples/webinar-roms-usecase-download/docs/rectilinear-angle-repair.json
```

An existing output root is rejected. Each derived file contains the retained
adapter fields, zero angle, and the original angle as `source_angle`. The
original download is never modified. `provenance/repair.json` records each
source and derived SHA-256, geometry residuals, fields verified unchanged and
completion state. Use the derived root as the explicit conversion base path;
do not substitute it silently for the original download archive.

For one native preparation conversion (not the repeated performance experiment):

```sh
data/wacomm-sarno-lite/venv/bin/python examples/webinar-roms-usecase-download/tools/convert_repaired.py \
  --forcing-root examples/webinar-roms-usecase-download/data/roms-20260915-16-angle-repaired-001 \
  --run-root examples/webinar-roms-usecase-download/data/native-conversion-angle-repaired-001 \
  --binary build-serial-rotation/wacommplusplus
```

The wrapper requires a completed repair manifest, verifies each derived hash,
archives the serial executable, configuration, build options and working diff,
and records output hashes. Pass its `processed/` directory to the native
example's preparation tool for field validation and two serial smoke replays.

The [executed repair record](angle-repair-results.json) confirms all 25 files,
56,388,777,875 derived bytes, zero coordinate residual, and exact retained-field
comparisons. The original download remains the provenance source.

## Verification and tolerances

Every source checksum must match the download manifest. Every retained field
except angle is compared exactly after writing, including physical time and
velocity. The regression test also verifies unchanged original bytes and
rejects inconsistent geometry, unsupported metadata and stretching. Run:

```sh
data/wacomm-sarno-lite/venv/bin/python examples/webinar-roms-usecase-download/tools/AngleRepairTest.py \
  build-serial-rotation/wacommplusplus
```

The application fixture checks that repaired U=2, V=6 m/s remains eastward 2,
northward 6 m/s within 10⁻⁶ m/s. Shared adapter tests cover rotation,
forward/backward execution and deterministic/seeded restart equivalence. The
repair is independent of tracking direction and does not alter time or seeds.

## Limitations and reproducibility metadata

This is an explicit, bounded metadata interpretation, not provider confirmation
of velocity semantics or observational validation. Any upstream dynamical error
caused by angle use inside ROMS remains outside the scope of this repair. Other
variables not required by this preparation are omitted from the derived subset.
Retain the original download manifest, repair policy and script copies, derived
checksums, Git revision and working diff, Python/NumPy/netCDF4 versions, resolved
conversion configuration and executable checksum with subsequent run archives.
Successful repair does not complete native smoke tests or a performance matrix.

## References

- Shchepetkin, A. F., and McWilliams, J. C. (2005). The regional oceanic modeling
  system (ROMS): a split-explicit, free-surface, topography-following-coordinate
  oceanic model. *Ocean Modelling*, 9(4), 347–404.
  [doi:10.1016/j.ocemod.2004.08.002](https://doi.org/10.1016/j.ocemod.2004.08.002).
