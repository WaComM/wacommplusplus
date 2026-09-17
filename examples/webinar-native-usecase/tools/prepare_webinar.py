#!/usr/bin/env python3
"""Stage declared native webinar forcing and verify a reduced-count serial replay."""
import argparse
import datetime
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys

EXAMPLE = Path(__file__).resolve().parents[1]
REPOSITORY = EXAMPLE.parents[1]
sys.path.insert(0, str(REPOSITORY / 'tools'))
from performance_evidence import sha, gridded_compare
from compare_particle_snapshots import compare
from webinar_protocol_collect import check_output_coverage
import netCDF4
import numpy as np


def validate_fields(dataset):
    horizontal = ('eta_rho', 'xi_rho' if 'xi_rho' in dataset.dimensions else 'eta_xi')
    schema = {
        'lat_rho': (horizontal, 'degree_north'), 'lon_rho': (horizontal, 'degree_east'),
        'mask_rho': (horizontal, '1'), 'h': (horizontal, 'meter'),
        'zeta': (('ocean_time',) + horizontal, 'meter'),
        'u': (('ocean_time', 's_rho') + horizontal, 'meter second-1'),
        'v': (('ocean_time', 's_rho') + horizontal, 'meter second-1'),
        'w': (('ocean_time', 's_w') + horizontal, 'meter second-1'),
        'akt': (('ocean_time', 's_w') + horizontal, 'meter2 second-1'),
    }
    for name, (dimensions, units) in schema.items():
        variable = dataset.variables[name]
        if variable.dimensions != dimensions or getattr(variable, 'units', getattr(variable, 'unit', None)) != units:
            raise ValueError(f'{name}: incompatible native dimensions or units')
        values = np.ma.asarray(variable[:])
        if np.ma.getmaskarray(values).any() or not np.isfinite(values).all():
            raise ValueError(f'{name}: missing or non-finite native values')
        if name == 'lat_rho' and ((values < -90).any() or (values > 90).any()):
            raise ValueError('latitude outside geographic domain')
        if name == 'lon_rho' and ((values < -180).any() or (values > 180).any()):
            raise ValueError('longitude outside declared -180..180 convention')
        if name == 'mask_rho' and not np.isin(values, [0, 1]).all():
            raise ValueError('mask must contain only zero and one')
        if name == 'akt' and (values < 0).any():
            raise ValueError('negative diffusivity')
    for name in ('s_rho', 's_w'):
        variable = dataset.variables[name]
        values = np.ma.asarray(variable[:])
        if (variable.dimensions != (name,) or getattr(variable, 'positive', None) != 'up' or
                getattr(variable, 'standard_name', None) != 'ocean_sigma_coordinates' or
                np.ma.getmaskarray(values).any() or not np.isfinite(values).all() or
                not (np.diff(values) > 0).all() or (values < -1).any() or (values > 0).any()):
            raise ValueError(f'{name}: unsupported native sigma coordinates')
    wet = dataset.variables['mask_rho'][:] == 1
    if not wet.any() or (dataset.variables['h'][:][wet] <= 0).any():
        raise ValueError('positive bathymetry and at least one wet cell required')


def validate_inputs(directory):
    config = json.loads((EXAMPLE / 'webinar-native-usecase.json').read_text())
    start = datetime.datetime.strptime(config['simulation']['start'], '%Y%m%dZ%H%M')
    end = datetime.datetime.strptime(config['simulation']['end'], '%Y%m%dZ%H%M')
    expected = [start + datetime.timedelta(hours=h) for h in range(int((end-start).total_seconds()/3600)+1)]
    observed = []
    files = [directory / name for name in config['io']['nc_inputs']]
    for index, path in enumerate(files):
        with netCDF4.Dataset(path) as dataset:
            validate_fields(dataset)
            time = dataset.variables['ocean_time']
            if time.dimensions != ('ocean_time',) or not hasattr(time, 'units') or not hasattr(time, 'calendar'):
                raise ValueError(f'{path}: explicit one-dimensional time units/calendar required')
            if time.units != 'seconds since 1968-05-23 00:00:00 GMT' or time.calendar != 'gregorian':
                raise ValueError('native reader requires the declared seconds epoch and Gregorian calendar; no implicit conversion')
            values = np.ma.asarray(time[:])
            if np.ma.getmaskarray(values).any() or not np.isfinite(values).all():
                raise ValueError(f'{path}: invalid time values')
            if not (np.diff(values) > 0).all():
                raise ValueError('native time must be strictly increasing within each file')
            epoch = datetime.datetime(1968, 5, 23)
            allowed = [(date - epoch).total_seconds() for date in expected[index:index+2]]
            if list(values) not in [allowed[:1], allowed]:
                raise ValueError(f'{path}: wrong hourly record or adjacent boundary')
            dates = netCDF4.num2date(values, time.units, time.calendar)
            observed.extend(datetime.datetime(d.year, d.month, d.day, d.hour, d.minute, d.second) for d in dates)
    if sorted(set(observed)) != expected or observed != sorted(observed):
        raise ValueError('forcing must cover exactly the declared hourly UTC boundaries in physical order')
    return config, files


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--forcing-dir', required=True, type=Path)
    parser.add_argument('--binary', required=True, type=Path, help='Release serial application')
    parser.add_argument('--timeout', type=float, default=600)
    args = parser.parse_args()
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error('timeout must be positive and finite')
    config, files = validate_inputs(args.forcing_dir.resolve())
    binary = args.binary.resolve(strict=True)
    cache = binary.parent / 'CMakeCache.txt'
    options = cache.read_text().splitlines()
    if any(f'USE_{backend}:BOOL=OFF' not in options for backend in ('MPI', 'OMP', 'CUDA')):
        raise ValueError('preparation requires a serial build with MPI/OpenMP/CUDA disabled')
    root = EXAMPLE / 'data'
    preparation = root / 'preparation'
    if preparation.exists() or (root / 'processed').exists():
        raise ValueError('preparation already exists; archive it before a new preparation')
    provenance = preparation / 'provenance'
    provenance.mkdir(parents=True)
    (root / 'processed').mkdir()
    for path in files:
        shutil.copy2(path, root / 'processed' / path.name)
    manifest = ''.join(f'{sha(root / "processed" / path.name)}  processed/{path.name}\n' for path in files)
    (provenance / 'prepared-forcing.sha256').write_text(manifest)
    (provenance / 'input-location.txt').write_text(str(args.forcing_dir.resolve()) + '\n')
    (provenance / 'revision.txt').write_bytes(subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPOSITORY))
    (provenance / 'working-tree.patch').write_bytes(subprocess.check_output(['git', 'diff', 'HEAD'], cwd=REPOSITORY))
    shutil.copy2(binary, preparation / 'wacommplusplus')
    (provenance / 'binary.sha256').write_text(sha(preparation / 'wacommplusplus') + '\n')
    if (binary.parent / 'CMakeCache.txt').is_file():
        shutil.copy2(binary.parent / 'CMakeCache.txt', provenance)
    source = json.loads((REPOSITORY / 'examples/sources-webinar/sources-webinar.json').read_text())
    source['features'][0]['properties']['emission'] = {
        'mode': 'uniform_rate', 'rate': 1000, 'rate_unit': 'particles/hour'}
    for repetition in (1, 2):
        sample = preparation / f'smoke-{repetition}'
        (sample / 'examples/sources-webinar').mkdir(parents=True)
        (sample / 'processed').symlink_to(root / 'processed', target_is_directory=True)
        (sample / 'output').mkdir()
        (sample / 'restart').mkdir()
        (sample / 'webinar-native-usecase.json').write_text(json.dumps(config, indent=2) + '\n')
        (sample / 'examples/sources-webinar/sources-webinar.json').write_text(json.dumps(source, indent=2) + '\n')
        with (sample / 'run.out').open('w') as stdout, (sample / 'run.err').open('w') as stderr:
            result = subprocess.run([str(preparation / 'wacommplusplus'), 'webinar-native-usecase.json'],
                                    cwd=sample, env=dict(os.environ, CUDA_VISIBLE_DEVICES='-1', OMP_NUM_THREADS='1'),
                                    stdout=stdout, stderr=stderr, timeout=args.timeout)
        (sample / 'exit.txt').write_text(str(result.returncode) + '\n')
        result.check_returncode()
    a, b = preparation / 'smoke-1', preparation / 'smoke-2'
    check_output_coverage(a)
    check_output_coverage(b)
    report = {'particles': compare(a / 'restart', b / 'restart'),
              'gridded': gridded_compare(a / 'output', b / 'output')}
    if not any(record['particles'] > 0 for record in report['particles']['snapshots']):
        raise ValueError('smoke run produced no particles')
    (provenance / 'smoke-comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    (provenance / 'smoke-passed.txt').write_text('Two serial seeded replays agree exactly.\n')
    print(preparation)


if __name__ == '__main__':
    main()
