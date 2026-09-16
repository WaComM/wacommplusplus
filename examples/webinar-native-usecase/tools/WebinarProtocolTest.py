#!/usr/bin/env python3
"""Verify day-boundary coverage and reject incomplete or misdeclared timing evidence."""
import datetime
import json
from pathlib import Path
import sys
import tempfile

try:
    import netCDF4
except ImportError:
    raise SystemExit(77)

sys.path.insert(0, str(Path(__file__).resolve().parent))
from webinar_protocol_collect import INTERVALS, solver_seconds
from prepare_webinar import validate_inputs, EXAMPLE


def rejected(function):
    try:
        function()
    except (ValueError, OSError, KeyError):
        return
    raise AssertionError('invalid evidence accepted')


def main():
    native = json.loads((EXAMPLE / 'webinar-native-usecase.json').read_text())
    roms = json.loads((EXAMPLE.parent / 'webinar-roms-usecase-download/webinar-roms-usecase-download.json').read_text())
    assert native['simulation']['start'] == roms['simulation']['start'] == '20260915Z0000'
    assert native['simulation']['end'] == roms['simulation']['end'] == '20260916Z0000'
    start = datetime.datetime(2026, 9, 15)
    times = [start + datetime.timedelta(hours=h) for h in range(25)]
    assert native['io']['nc_inputs'] == [t.strftime('ocm3_d03_%Y%m%dZ%H.nc') for t in times]
    assert roms['io']['nc_inputs'] == [t.strftime('%Y/%m/%d/rms3_d03_%Y%m%dZ%H00.nc') for t in times]
    assert native['physics']['random_sources'] and native['restart']['interval'] == 7200
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        log = root / 'run.out'
        header = 'Using 1/2 processes, each on 4 threads.\nAcceleration: CUDA 0 device(s)\n'
        lines = [f'Solver interval: start={a} end={b} seconds=1.25' for a, b in zip(INTERVALS[:-1], INTERVALS[1:])]
        log.write_text(header + '\n'.join(lines))
        assert solver_seconds(log, 2, 4, 0) == 30
        rejected(lambda: solver_seconds(log, 2, 8, 0))
        for invalid in [lines[:-1], lines + [lines[-1]], list(reversed(lines)),
                        [line.replace('seconds=1.25', 'seconds=nan') for line in lines]]:
            log.write_text(header + '\n'.join(invalid))
            rejected(lambda: solver_seconds(log, 2, 4, 0))
        rejected(lambda: validate_inputs(root))
        for index, name in enumerate(native['io']['nc_inputs']):
            with netCDF4.Dataset(root / name, 'w') as ds:
                for dim, size in [('ocean_time', 1), ('eta_rho', 2), ('xi_rho', 2), ('s_rho', 2), ('s_w', 3)]:
                    ds.createDimension(dim, size)
                clock = ds.createVariable('ocean_time', 'f8', ('ocean_time',))
                clock.units = 'seconds since 1968-05-23 00:00:00 GMT'
                clock.calendar = 'gregorian'
                clock[:] = [INTERVALS[index]]
                horizontal = ('eta_rho', 'xi_rho')
                for name, dimensions, units, value in [
                        ('lat_rho', horizontal, 'degree_north', 40.8),
                        ('lon_rho', horizontal, 'degree_east', 14.0),
                        ('h', horizontal, 'meter', 100), ('mask_rho', horizontal, '1', 1),
                        ('zeta', ('ocean_time',) + horizontal, 'meter', 0),
                        ('u', ('ocean_time', 's_rho') + horizontal, 'meter second-1', 0),
                        ('v', ('ocean_time', 's_rho') + horizontal, 'meter second-1', 0),
                        ('w', ('ocean_time', 's_w') + horizontal, 'meter second-1', 0),
                        ('akt', ('ocean_time', 's_w') + horizontal, 'meter2 second-1', 0)]:
                    variable = ds.createVariable(name, 'f8', dimensions)
                    variable.units = units
                    variable[:] = value
                for name, values in [('s_rho', [-0.75, -0.25]), ('s_w', [-1, -0.5, 0])]:
                    variable = ds.createVariable(name, 'f8', (name,))
                    variable.positive = 'up'
                    variable.standard_name = 'ocean_sigma_coordinates'
                    variable[:] = values
        assert len(validate_inputs(root)[1]) == 25
        first = root / native['io']['nc_inputs'][0]
        with netCDF4.Dataset(first, 'a') as ds:
            ds.variables['ocean_time'][:] = [INTERVALS[0] + 0.5]
        rejected(lambda: validate_inputs(root))
        with netCDF4.Dataset(first, 'a') as ds:
            ds.variables['ocean_time'][:] = [INTERVALS[0]]
            ds.variables['ocean_time'].units = 'hours since 1968-05-23 00:00:00 GMT'
        rejected(lambda: validate_inputs(root))
        with netCDF4.Dataset(first, 'a') as ds:
            ds.variables['ocean_time'].units = 'seconds since 1968-05-23 00:00:00 GMT'
            ds.variables['u'].units = 'centimeter second-1'
        rejected(lambda: validate_inputs(root))

    print('Webinar 25-file window and 24-interval evidence checks passed')


if __name__ == '__main__':
    main()
