#!/usr/bin/env python3
"""Produce an immutable ROMS subset with an explicitly validated angle repair."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil

import netCDF4
import numpy as np

EXAMPLE = Path(__file__).resolve().parents[1]
FIELDS = ('ocean_time', 's_rho', 's_w', 'lat_rho', 'lon_rho', 'lat_u', 'lon_u',
          'lat_v', 'lon_v', 'mask_rho', 'mask_u', 'mask_v', 'h', 'zeta', 'u', 'v',
          'w', 'AKt', 'angle', 'Cs_r', 'Cs_w', 'spherical', 'Vtransform',
          'Vstretching', 'theta_s', 'theta_b', 'hc')


def checksum(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def finite(variable):
    data = np.ma.asarray(variable[:])
    if np.ma.getmaskarray(data).any() or not np.isfinite(data).all():
        raise ValueError(f'{variable.name}: missing or non-finite geometry')
    return np.asarray(data)


def validate_policy(policy):
    expected = json.loads((EXAMPLE / 'docs/rectilinear-angle-repair.json').read_text())
    if policy != expected:
        raise ValueError('unsupported repair policy; this operator accepts only the documented explicit policy')


def validate_grid(ds, policy):
    validate_policy(policy)
    tolerance = policy['coordinate_tolerance_degrees']
    if finite(ds['spherical']).item() != 1:
        raise ValueError('explicit spherical ROMS grid required')
    dimensions = {
        'rho': ('eta_rho', 'xi_rho'), 'u': ('eta_u', 'xi_u'), 'v': ('eta_v', 'xi_v')}
    coords = {}
    for grid, dims in dimensions.items():
        for axis, units in [('lon', 'degree_east'), ('lat', 'degree_north')]:
            name = axis + '_' + grid
            variable = ds[name]
            if variable.dimensions != dims or getattr(variable, 'units', None) != units:
                raise ValueError(f'{name}: incorrect dimensions or declared angular units')
            coords[name] = finite(variable)
    lon, lat = coords['lon_rho'], coords['lat_rho']
    if min(lon.shape) < 2 or lon.shape != lat.shape:
        raise ValueError('insufficient rho geometry')
    if (lon < -180).any() or (lon > 180).any() or (lat <= -90).any() or (lat >= 90).any():
        raise ValueError('outside declared spherical geographic domain')
    if not (np.diff(lon, axis=1) > 0).all() or not (np.diff(lat, axis=0) > 0).all():
        raise ValueError('XI must increase eastward and ETA northward; cyclic or reversed grids unsupported')
    errors = {'rho_longitude_cross_axis': float(np.abs(lon-lon[0:1, :]).max()),
              'rho_latitude_cross_axis': float(np.abs(lat-lat[:, 0:1]).max())}
    expected = {'lon_u': .5*(lon[:, :-1]+lon[:, 1:]),
                'lat_u': .5*(lat[:, :-1]+lat[:, 1:]),
                'lon_v': .5*(lon[:-1, :]+lon[1:, :]),
                'lat_v': .5*(lat[:-1, :]+lat[1:, :])}
    for name, value in expected.items():
        if coords[name].shape != value.shape:
            raise ValueError(f'{name}: incompatible C-grid staggering')
        errors[name] = float(np.abs(coords[name]-value).max())
    if any(value > tolerance for value in errors.values()):
        raise ValueError(f'non-rectilinear or inconsistent C-grid: {errors}')
    for name, grid, location in [('u', 'u', 'edge1'), ('v', 'v', 'edge2')]:
        variable = ds[name]
        if (variable.dimensions != ('ocean_time', 's_rho') + dimensions[grid] or
                getattr(variable, 'units', None) != 'meter second-1' or
                getattr(variable, 'location', None) != location or
                getattr(variable, 'standard_name', '') or
                any(attr in variable.ncattrs() for attr in ('scale_factor', 'add_offset'))):
            raise ValueError(f'{name}: declared unpacked ROMS grid-axis momentum required')
    angle = ds['angle']
    if angle.dimensions != dimensions['rho'] or getattr(angle, 'units', None) != 'radians':
        raise ValueError('angle must be a declared rho-point radian field')
    discrepancy = np.abs(finite(angle)-policy['expected_source_angle_radians'])
    if float(discrepancy.max()) > policy['source_angle_tolerance_radians']:
        raise ValueError('source angle differs from this repair policy')
    for level, curve in [('s_rho', 'Cs_r'), ('s_w', 'Cs_w')]:
        if not np.array_equal(finite(ds[level]), finite(ds[curve])):
            raise ValueError('stretched vertical coordinates are unsupported by this workflow')
    return errors


def copy_verified(source, target, policy, policy_hash):
    with netCDF4.Dataset(source) as original:
        errors = validate_grid(original, policy)
        original.set_auto_maskandscale(False)
        with netCDF4.Dataset(target, 'w', format='NETCDF4') as derived:
            for name, dimension in original.dimensions.items():
                if any(name in original[field].dimensions for field in FIELDS):
                    derived.createDimension(name, len(dimension))
            derived.setncatts({name: original.getncattr(name) for name in original.ncattrs()})
            derived.setncattr('wacomm_angle_repair_operator', policy['operator'])
            derived.setncattr('wacomm_angle_repair_policy_sha256', policy_hash)
            derived.setncattr('wacomm_angle_repair_interpretation', policy['interpretation'])
            verified = []
            for name in FIELDS:
                variable = original[name]
                attributes = {key: variable.getncattr(key) for key in variable.ncattrs()}
                fill = attributes.pop('_FillValue', None)
                options = {} if fill is None else {'fill_value': fill}
                output = derived.createVariable(name, variable.dtype, variable.dimensions, **options)
                output.setncatts(attributes)
                output.set_auto_maskandscale(False)
                if name == 'angle':
                    output[:] = policy['replacement_angle_radians']
                    prior = derived.createVariable('source_angle', variable.dtype, variable.dimensions, **options)
                    prior.setncatts(attributes)
                    prior[:] = variable[:]
                    continue
                # Read, write and compare bounded slabs, preserving stored values exactly.
                if variable.ndim < 2:
                    selections = [Ellipsis]
                else:
                    selections = [(slice(index, min(index+16, variable.shape[0])),) +
                                  (slice(None),)*(variable.ndim-1) for index in range(0, variable.shape[0], 16)]
                    if variable.ndim == 4:
                        selections = [(slice(t, t+1), slice(k, k+1), slice(None), slice(None))
                                      for t in range(variable.shape[0]) for k in range(variable.shape[1])]
                for selection in selections:
                    values = variable[selection]
                    output[selection] = values
                    if not np.array_equal(values, output[selection], equal_nan=True):
                        raise ValueError(f'{name}: stored values changed in the derived file')
                verified.append(name)
            derived.sync()
    return {'geometry_errors_degrees': errors, 'unchanged_fields_verified': verified,
            'replacement_angle_radians': policy['replacement_angle_radians']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', required=True, type=Path)
    parser.add_argument('--output-root', required=True, type=Path)
    parser.add_argument('--policy', required=True, type=Path)
    parser.add_argument('--configuration', type=Path, default=EXAMPLE / 'webinar-roms-usecase-download.json')
    args = parser.parse_args()
    policy = json.loads(args.policy.read_text())
    validate_policy(policy)
    config = json.loads(args.configuration.read_text())
    root = args.output_root.resolve()
    root.mkdir(parents=True, exist_ok=False)
    provenance = root / 'provenance'
    provenance.mkdir()
    shutil.copy2(args.policy, provenance / 'policy.json')
    shutil.copy2(__file__, provenance / 'repair_angle.py')
    manifest = json.loads((args.source_root / 'provenance/download.json').read_text())
    record = {'operator': policy['operator'], 'policy': policy, 'source_root': str(args.source_root.resolve()),
              'created_utc': datetime.now(timezone.utc).isoformat(), 'files': [], 'complete': False}
    path = provenance / 'repair.json'
    path.write_text(json.dumps(record, indent=2)+'\n')
    for name in config['io']['nc_inputs']:
        source = args.source_root / name
        expected = manifest['files'][name]
        if source.stat().st_size != expected['bytes'] or checksum(source) != expected['sha256']:
            raise ValueError(f'{name}: original download checksum mismatch')
        target = root / name
        target.parent.mkdir(parents=True, exist_ok=True)
        partial = target.with_suffix('.nc.part')
        result = copy_verified(source, partial, policy, checksum(args.policy))
        result.update(name=name, source_sha256=expected['sha256'], derived_sha256=checksum(partial),
                      derived_bytes=partial.stat().st_size)
        partial.rename(target)
        record['files'].append(result)
        path.write_text(json.dumps(record, indent=2)+'\n')
        print(f'Verified derived file {len(record["files"])}/{len(config["io"]["nc_inputs"])}: {name}', flush=True)
    record['complete'] = True
    path.write_text(json.dumps(record, indent=2)+'\n')
    print(path, flush=True)


if __name__ == '__main__':
    main()
