#!/usr/bin/env python3
import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import performance_protocol as protocol


def make(root, k, seconds):
    p, n, g = k
    directory = root / f'p{p}_n{n}_g{g}'
    directory.mkdir()
    (directory / 'validation.md').write_text('Verified particle states at stable ID and physical time.\n')
    record = dict(mpi_processes=p, openmp_threads=n, gpu_devices=g,
                  solver_seconds=[seconds] * 3, exit_code=0,
                  equivalence_passed=True, validation_report='validation.md',
                  revision='abc', configuration_sha256='config',
                  forcing_sha256='forcing', binary_sha256='binary',
                  hardware_id='node', timing_scope='solver')
    (directory / 'run.json').write_text(json.dumps(record))


def main():
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        for k in protocol.CPU:
            make(root, k, 100 / (k[0] * k[1]) + (0 if k == (4, 8, 0) else 10))
        records, best, gpu_reference, complete = protocol.load(root)
        assert best == (4, 8, 0) and gpu_reference == best and not complete
        assert len(records) == 16
        for g in range(1, 5):
            make(root, (4, 8, g), 3 / (g + 1))
        records, best, gpu_reference, complete = protocol.load(root)
        assert complete and best == (4, 8, 0)
        for record in records:
            protocol.write_note(Path(record['source_path']).parent / 'codex-performance-review.md', record, best, gpu_reference)
        assert len(list(root.glob('*/codex-performance-review.md'))) == 20
        try:
            protocol.plot(root, records, gpu_reference, complete)
        except ImportError:
            pass
        else:
            assert (root / 'speedup.svg').is_file()
            assert (root / 'cpu_efficiency.svg').is_file()
            assert (root / 'gpu_incremental_speedup.svg').is_file()
            assert (root / 'gpu_device_scaling.svg').is_file()
        bad = root / 'p4_n8_g1' / 'run.json'
        data = json.loads(bad.read_text())
        data['solver_seconds'] = [-1, 1, 1]
        bad.write_text(json.dumps(data))
        try:
            protocol.load(root)
        except ValueError:
            pass
        else:
            raise AssertionError('nonpositive timings accepted')

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        for k in protocol.CPU:
            seconds = 1 if k == (64, 1, 0) else 2 if k == (2, 16, 0) else 20
            make(root, k, seconds)
        records, best, gpu_reference, complete = protocol.load(root)
        assert best == (64, 1, 0)
        assert gpu_reference == (2, 16, 0)
        assert not complete


if __name__ == '__main__':
    main()
