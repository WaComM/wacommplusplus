#!/usr/bin/env python3
"""Validate Sarno Slurm runs and produce inputs for the shared protocol collector."""
import argparse
import datetime
import hashlib
import json
import math
from pathlib import Path
import re
import sys

import netCDF4
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tools"))
from compare_particle_snapshots import compare, snapshot_index, order
import performance_protocol

TIMES = [datetime.datetime(2021, 7, 1, h, tzinfo=datetime.timezone.utc)
         for h in (9, 10, 11, 13, 14, 15)]
EPOCH = datetime.datetime(1968, 5, 23, tzinfo=datetime.timezone.utc)
INTERVALS = [(a - EPOCH).total_seconds() for a in TIMES]


from performance_evidence import sha, gpu_compare, gridded_compare, hardware_id, job_records
from performance_evidence import solver_seconds as interval_seconds


def solver_seconds(path, p, n, g):
    return interval_seconds(path, p, n, g, INTERVALS)


def process(root, only_run=None):
    reference_sample = root / 'p1_n1_g0' / 'sample-1'
    reference = reference_sample / 'snapshots-6h'
    if not reference.is_dir():
        raise ValueError('baseline snapshots absent')
    forcing_manifest = root / 'provenance' / 'forcing.sha256'
    particles_per_hour = int((root / 'provenance' / 'particles-per-hour.txt').read_text())
    if particles_per_hour <= 0:
        raise ValueError('particles per hour must be positive')
    source_manifest = root / 'provenance' / 'source.json'
    source_document = json.loads(source_manifest.read_text())
    emission = source_document.get('features', [{}])[0].get('properties', {}).get('emission', {})
    if (len(source_document.get('features', [])) != 1 or
            emission.get('mode') != 'uniform_rate' or emission.get('rate') != particles_per_hour or
            emission.get('rate_unit') != 'particles/hour'):
        raise ValueError('source manifest disagrees with declared emission rate')
    identity = None
    for run in sorted(root.glob('p*_n*_g*')):
        if only_run is not None and run.name != only_run:
            continue
        p, n, g = map(int, re.fullmatch(r'p(\d+)_n(\d+)_g(\d+)', run.name).groups())
        if (run / 'run.json').is_file() and (run / 'validation.md').is_file():
            continue
        if not (run / 'sample-3' / 'exit.txt').is_file():
            raise ValueError(f'{run}: incomplete job')
        samples = []
        application_samples = []
        comparisons = []
        config = run / 'sample-1' / 'wacomm-sarno-lite-6h.json'
        source = run / 'sample-1' / 'examples/sources-sarno_river/sources-sarno_river.json'
        for rep in (1, 2, 3):
            sample = run / f'sample-{rep}'
            if (sample / 'exit.txt').read_text().strip() != '0':
                raise ValueError(f'{sample}: failed model')
            if sha(sample / 'wacomm-sarno-lite-6h.json') != sha(config) or sha(sample / 'examples/sources-sarno_river/sources-sarno_river.json') != sha(source):
                raise ValueError(f'{sample}: changing scientific inputs')
            samples.append(solver_seconds(sample / 'run.out', p, n, g))
            elapsed = float((sample / 'elapsed-seconds.txt').read_text())
            if not math.isfinite(elapsed) or elapsed <= 0:
                raise ValueError(f'{sample}: invalid application duration')
            application_samples.append(elapsed)
            if g and (not (sample / 'gpu-monitor.log').is_file() or
                      (sample / 'gpu-monitor.log').stat().st_size == 0):
                raise ValueError(f'{sample}: missing GPU utilization/memory/power record')
            snapshots = sample / 'snapshots-6h'
            try:
                particle_report = gpu_compare(reference, snapshots) if g else compare(reference, snapshots)
                grid_report = gridded_compare(reference_sample / 'output-6h', sample / 'output-6h')
            except (ValueError, OSError, RuntimeError) as error:
                raise ValueError(f'{sample}: {error}') from error
            comparisons.append({'particles': particle_report, 'gridded': grid_report})
        bin_hash = sha(run / 'wacommplusplus')
        signature = (sha(config), sha(source), bin_hash, sha(forcing_manifest))
        if signature[1] != sha(source_manifest):
            raise ValueError(f'{run}: source differs from suite emission-rate manifest')
        if identity is None:
            identity = signature
        elif identity != signature:
            raise ValueError(f'{run}: inconsistent binary/configuration/source/forcing')
        validation = run / 'validation.md'
        used_jobs, superseded_jobs = job_records(run)
        device_mask = re.search(r'^CUDA_VISIBLE_DEVICES=(.*)$',
                                (run / 'provenance/runtime.txt').read_text(), re.MULTILINE)
        if not device_mask:
            raise ValueError(f'{run}: missing CUDA device mask')
        lines = [f'# Sarno validation: {p}/{n}/{g}', '',
                 f'Passive six-hour release at {particles_per_hour} particles per hour, 2021-07-01 09:00–15:00 UTC; seed 5489.',
                 'All measured snapshots were compared by integer particle ID and physical time.',
                 'GPU coordinates use the absolute tolerances defined below; CPU comparisons require exact equality.',
                 '', f'- Measured job IDs by repetition: {used_jobs}',
                 f'- Superseded or failed job IDs: {superseded_jobs}',
                 f'- Config SHA-256: {signature[0]}', f'- Sources SHA-256: {signature[1]}',
                 f'- Binary SHA-256: {signature[2]}', f'- Forcing manifest SHA-256: {signature[3]}',
                 f'- Hardware class: {hardware_id(run)}',
                 f'- CUDA_VISIBLE_DEVICES: {device_mask.group(1)}',
                 f'- Solver sample durations (s): {samples}',
                 f'- Application sample durations (s): {application_samples}',
                 '- Complete raw logs, Slurm settings, binding, outputs and checksums are in this run directory.',
                 '- GPU runs additionally archive `provenance/gpu-topology.txt`, `provenance/gpu-details.txt`, and per-sample `gpu-monitor.log`.',
                 '', '## Comparison report', '', '```json', json.dumps(comparisons, indent=2), '```', '']
        validation.write_text('\n'.join(lines))
        record = {'mpi_processes': p, 'openmp_threads': n, 'gpu_devices': g,
                  'problem_size_particles_per_hour': particles_per_hour,
                  'problem_size': particles_per_hour,
                  'problem_size_unit': 'particles/hour',
                  'solver_seconds': samples, 'application_seconds': application_samples,
                  'exit_code': 0, 'equivalence_passed': True,
                  'validation_report': 'validation.md',
                  'revision': (root / 'provenance/revision.txt').read_text().strip(),
                  'configuration_sha256': signature[0], 'source_sha256': signature[1],
                  'forcing_sha256': signature[3],
                  'binary_sha256': signature[2], 'hardware_id': hardware_id(run),
                  'timing_scope': 'solver'}
        (run / 'run.json').write_text(json.dumps(record, indent=2) + '\n')
    if only_run is not None:
        return None
    records, best, gpu_reference, gpu_complete = performance_protocol.load(root)
    (root / 'results.json').write_text(json.dumps({'selected_cpu': best,
                                                  'gpu_reference_cpu': gpu_reference,
                                                  'gpu_sweep_complete': gpu_complete,
                                                  'records': records}, indent=2) + '\n')
    for record in records:
        performance_protocol.write_note(Path(record['source_path']).parent / 'codex-performance-review.md', record, best, gpu_reference)
    performance_protocol.plot(root, records, gpu_reference, gpu_complete)
    return best, gpu_reference, gpu_complete


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_root', type=Path)
    parser.add_argument('--run', help='validate one resource directory before aggregate collection')
    args = parser.parse_args()
    try:
        result = process(args.run_root, args.run)
    except (ValueError, OSError, RuntimeError) as error:
        match = re.search(r'p\d+_n\d+_g\d+', str(error))
        note_root = args.run_root / match.group() if match else args.run_root
        if note_root.is_dir():
            (note_root / 'codex-performance-review.md').write_text(
                '# Failed Sarno performance validation\n\n'
                f'Validation failed: `{error}`. Inspect this run’s raw output, Slurm binding, '
                'configuration, forcing and executable hashes, and the corresponding baseline. '
                'Diagnose the shared solver or example infrastructure and apply a generic fix only after '
                'backward/forward, restart, and backend-equivalence regression checks. '
                'Do not use this run in a speedup or efficiency chart.\n')
        parser.exit(2, str(error) + '\n')
    if result is None:
        print(f'Validated resource directory: {args.run}')
    else:
        best, gpu_reference, gpu_complete = result
        print(f'Selected CPU tuple: {best}; GPU-compatible CPU reference: {gpu_reference}; complete GPU sweep: {gpu_complete}')


if __name__ == '__main__':
    main()
