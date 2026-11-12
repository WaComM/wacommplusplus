#!/usr/bin/env python3
"""Publish compact, validated performance results and charts to a versioned directory."""
import argparse
import hashlib
import json
from pathlib import Path

import performance_protocol


FIGURES = ('speedup.svg', 'cpu_efficiency.svg',
           'gpu_incremental_speedup.svg', 'gpu_device_scaling.svg')
RECORD_FIELDS = ('mpi_processes', 'openmp_threads', 'gpu_devices',
                 'solver_seconds', 'median_seconds', 'application_seconds',
                 'speedup', 'cpu_efficiency', 'gpu_incremental_speedup',
                 'gpu_device_speedup', 'gpu_device_efficiency')


def publish(root, output, physical_window=None, excluded_jobs=()):
    records, selected, gpu_reference, complete = performance_protocol.load(root)
    if not complete:
        raise ValueError('a complete CPU and selected-CPU GPU sweep is required')
    sizes = {(r.get('problem_size'), r.get('problem_size_unit')) for r in records}
    if len(sizes) != 1 or None in next(iter(sizes)):
        raise ValueError('consistent problem-size metadata is required')
    provenance_fields = ('revision', 'configuration_sha256', 'source_sha256',
                         'forcing_sha256', 'binary_sha256', 'hardware_id')
    provenance = {field: records[0].get(field) for field in provenance_fields}
    if any(not value for value in provenance.values()):
        raise ValueError('complete provenance is required')
    if any(any(r.get(field) != value for field, value in provenance.items()) for r in records):
        raise ValueError('run provenance differs')
    for name in FIGURES:
        if not (root / name).is_file():
            raise ValueError(f'missing chart: {name}')
    if output.exists():
        raise ValueError(f'archive already exists: {output}')
    output.mkdir(parents=True)
    hashes = {}
    for name in FIGURES:
        lines = (root / name).read_text().splitlines()
        (output / name).write_text('\n'.join(line.rstrip(' \t') for line in lines) + '\n')
        hashes[name] = hashlib.sha256((output / name).read_bytes()).hexdigest()
    size, unit = next(iter(sizes))
    compact = {'schema': 'wacomm-performance-published-v1',
               'problem_size': size, 'problem_size_unit': unit,
               'physical_window': physical_window,
               'selected_cpu': selected, 'gpu_reference_cpu': gpu_reference,
               'gpu_sweep_complete': complete,
               'provenance': provenance, 'raw_archive': str(root),
               'failed_excluded_jobs': list(excluded_jobs),
               'records': [{field: r.get(field) for field in RECORD_FIELDS} for r in records],
               'figure_sha256': hashes}
    (output / 'results.json').write_text(json.dumps(compact, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_root', type=Path)
    parser.add_argument('output_dir', type=Path)
    parser.add_argument('--physical-window', help='documented physical-time interval')
    parser.add_argument('--excluded-job', action='append', type=int, default=[],
                        help='failed or superseded scheduler job ID excluded from timing')
    args = parser.parse_args()
    publish(args.run_root, args.output_dir, args.physical_window, args.excluded_job)


if __name__ == '__main__':
    main()
