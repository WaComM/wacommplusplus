#!/usr/bin/env python3
import sys
import tempfile
from pathlib import Path

try:
    import netCDF4
except ImportError:
    raise SystemExit(77)

sys.path.insert(0, str(Path(__file__).resolve().parent))
from sarno_protocol_collect import gpu_compare, gridded_compare, solver_seconds, INTERVALS
from partial_sarno_diagnostic import write_notes
from trajectory_diagnostics import TIME_UNITS


def snapshot(directory, longitude):
    directory.mkdir(exist_ok=True)
    with netCDF4.Dataset(directory / 'particles.nc', 'w') as ds:
        ds.createDimension('particles', 2)
        ds.createDimension('particle_time', 1)
        ds.createVariable('particle_time', 'f8', ('particle_time',))[:] = [INTERVALS[1]]
        ds.variables['particle_time'].units = TIME_UNITS
        ds.variables['particle_time'].calendar = 'gregorian'
        ds.createVariable('id', 'u8', ('particles',))[:] = [2, 1]
        for name, values in [('lat', [[40], [40]]), ('lon', [[longitude], [15]]),
                             ('depth', [[1], [2]]), ('i', [[1], [2]]),
                             ('j', [[1], [2]]), ('k', [[1], [2]]),
                             ('health', [[1], [1]]), ('age', [[3600], [3600]]),
                             ('time', [[INTERVALS[1]], [INTERVALS[1]]])]:
            ds.createVariable(name, 'f8', ('particles', 'particle_time'))[:] = values
        ds.createVariable('object_type', 'u4', ('particles',))[:] = [0, 0]
        ds.createVariable('drift_side', 'i4', ('particles',))[:] = [0, 0]


def grid(directory):
    directory.mkdir(exist_ok=True)
    with netCDF4.Dataset(directory / 'grid.nc', 'w') as ds:
        ds.createDimension('x', 2)
        ds.createVariable('conc', 'f4', ('x',))[:] = [1, 2]


def main():
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        a, b = root / 'a', root / 'b'
        snapshot(a, 15)
        snapshot(b, 15 + 1e-12)
        report = gpu_compare(a, b)
        assert report['snapshots'][0]['maxima']['horizontal_m'] < 1e-6
        (b / 'particles.nc').unlink()
        snapshot(b, 15 + 1e-3)
        try:
            gpu_compare(a, b)
        except ValueError:
            pass
        else:
            raise AssertionError('large GPU displacement accepted')
        grid(root / 'grid-a')
        grid(root / 'grid-b')
        assert len(gridded_compare(root / 'grid-a', root / 'grid-b')) == 1
        path = root / 'run.out'
        lines = [f'Solver interval: start={int(x)} end={int(y)} seconds=1.25' for x, y in zip(INTERVALS[:-1], INTERVALS[1:])]
        path.write_text('Using 1/2 processes, each on 4 threads.\nAcceleration: CUDA 0 device(s)\n' + '\n'.join(lines))
        assert solver_seconds(path, 2, 4, 0) == 6.25
        try:
            solver_seconds(path, 2, 8, 0)
        except ValueError:
            pass
        else:
            raise AssertionError('wrong thread count accepted')
        (root / 'provenance').mkdir()
        (root / 'p2_n4_g0').mkdir()
        report = {'problem_size_particles_per_hour': 1000, 'completed_cpu_tuples': 1,
                  'candidate': [2, 4, 0], 'revision': 'abc', 'hardware_id': 'node',
                  'records': [{'mpi_processes': 2, 'openmp_threads': 4,
                               'solver_seconds': [1, 2, 3], 'median_seconds': 2,
                               'scientific_equivalence': 'exact comparison passed'}]}
        write_notes(root, report)
        assert 'incomplete CPU matrix' in (root / 'p2_n4_g0/codex-performance-review.md').read_text()


if __name__ == '__main__':
    main()
