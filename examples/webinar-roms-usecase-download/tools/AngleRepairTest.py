#!/usr/bin/env python3
"""Verify the explicit rectilinear angle repair and reject unsupported geometry."""
import json
from pathlib import Path
import sys
import subprocess
import tempfile

try:
    import netCDF4
    import numpy as np
except ImportError:
    raise SystemExit(77)

sys.path.insert(0, str(Path(__file__).resolve().parent))
from repair_angle import EXAMPLE, FIELDS, copy_verified, validate_grid, checksum


def fixture(path):
    with netCDF4.Dataset(path, 'w') as ds:
        for name, size in [('ocean_time', 2), ('s_rho', 2), ('s_w', 3), ('eta_rho', 3),
                           ('xi_rho', 4), ('eta_u', 3), ('xi_u', 3), ('eta_v', 2), ('xi_v', 4)]:
            ds.createDimension(name, size)
        lon, lat = np.meshgrid([14., 14.1, 14.2, 14.3], [40., 40.1, 40.2])
        for grid, dims, x, y in [('rho', ('eta_rho', 'xi_rho'), lon, lat),
                                  ('u', ('eta_u', 'xi_u'), .5*(lon[:, :-1]+lon[:, 1:]), .5*(lat[:, :-1]+lat[:, 1:])),
                                  ('v', ('eta_v', 'xi_v'), .5*(lon[:-1, :]+lon[1:, :]), .5*(lat[:-1, :]+lat[1:, :]))]:
            for axis, units, values in [('lon', 'degree_east', x), ('lat', 'degree_north', y)]:
                var = ds.createVariable(axis+'_'+grid, 'f8', dims)
                var.units = units
                var[:] = values
            ds.createVariable('mask_'+grid, 'f8', dims)[:] = 1
        for name, dimension, values in [('s_rho', 's_rho', [-.75, -.25]), ('s_w', 's_w', [-1, -.5, 0]),
                                        ('Cs_r', 's_rho', [-.75, -.25]), ('Cs_w', 's_w', [-1, -.5, 0])]:
            ds.createVariable(name, 'f8', (dimension,))[:] = values
        time = ds.createVariable('ocean_time', 'f8', ('ocean_time',))
        time.units = 'seconds since 1968-05-23 00:00:00'
        time.calendar = 'gregorian'
        time[:] = [1840233600, 1840237200]
        for name, dims, units, value in [('angle', ('eta_rho', 'xi_rho'), 'radians', np.pi/2),
                                         ('h', ('eta_rho', 'xi_rho'), 'meter', 100),
                                         ('zeta', ('ocean_time', 'eta_rho', 'xi_rho'), 'meter', 0),
                                         ('u', ('ocean_time', 's_rho', 'eta_u', 'xi_u'), 'meter second-1', 2),
                                         ('v', ('ocean_time', 's_rho', 'eta_v', 'xi_v'), 'meter second-1', 6),
                                         ('w', ('ocean_time', 's_w', 'eta_rho', 'xi_rho'), 'meter second-1', 0),
                                         ('AKt', ('ocean_time', 's_w', 'eta_rho', 'xi_rho'), 'meter2 second-1', 0)]:
            var = ds.createVariable(name, 'f8', dims)
            var.units = units
            var[:] = value
            if name in ('u', 'v'):
                var.location = 'edge1' if name == 'u' else 'edge2'
        for name, value in [('spherical', 1), ('Vtransform', 1), ('Vstretching', 1), ('theta_s', 0), ('theta_b', 3), ('hc', 2.5)]:
            ds.createVariable(name, 'f8')[:] = value


def main():
    policy_path = EXAMPLE / 'docs/rectilinear-angle-repair.json'
    policy = json.loads(policy_path.read_text())
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        source, target = root/'original.nc', root/'derived.nc'
        fixture(source)
        before = checksum(source)
        report = copy_verified(source, target, policy, checksum(policy_path))
        assert checksum(source) == before
        assert report['replacement_angle_radians'] == 0
        with netCDF4.Dataset(source) as a, netCDF4.Dataset(target) as b:
            assert (b['angle'][:] == 0).all()
            assert np.array_equal(b['source_angle'][:], a['angle'][:])
            for name in FIELDS:
                if name != 'angle':
                    assert np.array_equal(a[name][:], b[name][:])
        if len(sys.argv) > 1:
            config = json.loads((EXAMPLE / 'webinar-roms-usecase-download.json').read_text())
            config['simulation']['end'] = '20260915Z0100'
            config['io']['base_path'] = str(root) + '/'
            config['io']['nc_inputs'] = ['derived.nc']
            config['io']['nc_input_root'] = str(root / 'native_')
            configuration = root / 'wacomm.json'
            configuration.write_text(json.dumps(config))
            subprocess.run([str(Path(sys.argv[1]).resolve()), str(configuration)], cwd=root, check=True,
                           stdout=subprocess.DEVNULL)
            outputs = list(root.glob('native_*.nc'))
            assert outputs, 'native conversion produced no output'
            for output in outputs:
                with netCDF4.Dataset(output) as ds:
                    assert np.allclose(ds['u'][:], 2, rtol=0, atol=1e-6)
                    assert np.allclose(ds['v'][:], 6, rtol=0, atol=1e-6)
        for variable, attribute, value in [('angle', 'units', 'degrees'), ('u', 'standard_name', 'eastward_sea_water_velocity'),
                                            ('u', 'scale_factor', 2.)]:
            fixture(source)
            with netCDF4.Dataset(source, 'a') as ds:
                ds[variable].setncattr(attribute, value)
            with netCDF4.Dataset(source) as ds:
                try:
                    validate_grid(ds, policy)
                except ValueError:
                    pass
                else:
                    raise AssertionError('unsupported metadata accepted')
        for variable in ('lat_rho', 'lon_u', 'angle', 'Cs_r'):
            fixture(source)
            with netCDF4.Dataset(source, 'a') as ds:
                values = ds[variable][:]
                values.flat[0] += .01
                ds[variable][:] = values
            with netCDF4.Dataset(source) as ds:
                try:
                    validate_grid(ds, policy)
                except ValueError:
                    pass
                else:
                    raise AssertionError('inconsistent geometry accepted')
    print('Angle repair preserves velocities, source bytes and time; invalid geometry/basis rejected')


if __name__ == '__main__':
    main()
