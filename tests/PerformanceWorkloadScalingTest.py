#!/usr/bin/env python3
"""Check that workload comparisons require complete, comparable sweeps."""
import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import performance_protocol as protocol
import performance_publish
import performance_workload_scaling as scaling


def make_case(root, size, best=(4, 8, 0), revision='abc'):
    for p, n, g in sorted(protocol.CPU | {(best[0], best[1], g) for g in range(1, 5)}):
        directory = root / f'p{p}_n{n}_g{g}'
        directory.mkdir(parents=True)
        (directory / 'validation.md').write_text('Particle and field equivalence passed.\n')
        seconds = size / (100 * p * n) + (0 if (p, n) == best[:2] else 10) + g
        record = dict(mpi_processes=p, openmp_threads=n, gpu_devices=g,
                      solver_seconds=[seconds] * 3, exit_code=0,
                      equivalence_passed=True, validation_report='validation.md',
                      revision=revision, configuration_sha256='config',
                      forcing_sha256='forcing', binary_sha256='binary',
                      hardware_id='node', timing_scope='solver',
                      problem_size=size, problem_size_unit='particles/hour',
                      source_sha256=f'source-{size}')
        (directory / 'run.json').write_text(json.dumps(record))


def main():
    with tempfile.TemporaryDirectory() as temporary:
        base = Path(temporary)
        cases = [(1000, base / 'small'), (10000, base / 'large')]
        for size, root in cases:
            make_case(root, size, revision=f'docs-only-{size}')
        summary = scaling.collect(cases)
        assert summary['cases'][0]['revision'] != summary['cases'][1]['revision']
        assert len(summary['cases']) == 2
        assert all(case['selected_cpu'] == (4, 8, 0) for case in summary['cases'])
        assert all(case['best_measured_configuration'] == (4, 8, 0) for case in summary['cases'])
        assert all(len(case['gpu_sweep']) == 5 for case in summary['cases'])
        scaling.write_report(summary, base)
        assert '| 1000 | 4/8/0 | 4/8/0 |' in (base / 'workload-summary.md').read_text()
        for name in performance_publish.FIGURES:
            (cases[0][1] / name).write_text('<svg xmlns="http://www.w3.org/2000/svg"/> \n')
        performance_publish.publish(cases[0][1], base / 'published', 'test interval')
        published = json.loads((base / 'published/results.json').read_text())
        assert published['problem_size'] == 1000 and len(published['records']) == 20
        assert len(published['figure_sha256']) == 4
        assert all(not line.endswith(' ') for line in (base / 'published/speedup.svg').read_text().splitlines())
        try:
            scaling.collect(cases + [cases[0]])
        except ValueError as error:
            assert 'duplicate' in str(error)
        else:
            raise AssertionError('duplicate size accepted')
        bad = cases[1][1] / 'p4_n8_g1' / 'run.json'
        record = json.loads(bad.read_text())
        record['problem_size'] = 1000
        bad.write_text(json.dumps(record))
        try:
            scaling.collect(cases)
        except ValueError as error:
            assert 'problem-size' in str(error)
        else:
            raise AssertionError('mismatched size accepted')


if __name__ == '__main__':
    main()
