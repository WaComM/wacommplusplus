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


def sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def solver_seconds(path, p, n, g):
    stdout = path.read_text()
    matches = re.findall(r'Solver interval: start=([^ ]+) end=([^ ]+) seconds=([^\s]+)', stdout)
    values = [(float(a), float(b), float(c)) for a, b, c in matches]
    if [(a, b) for a, b, _ in values] != list(zip(INTERVALS[:-1], INTERVALS[1:])):
        raise ValueError(f'{path}: missing, duplicate, or reordered solver intervals')
    if any(not math.isfinite(t) or t <= 0 for _, _, t in values):
        raise ValueError(f'{path}: invalid solver duration')
    if f'Using 1/{p} processes, each on {n} threads.' not in stdout:
        raise ValueError(f'{path}: wrong runtime process/thread count')
    accelerators = re.findall(r'Acceleration: CUDA (\d+) device\(s\)', stdout)
    if not accelerators or any(int(x) != g for x in accelerators):
        raise ValueError(f'{path}: expected {g} visible CUDA devices per rank')
    bindings = re.findall(r'^GPU binding: rank=(\d+) host=([^ ]+) devices=([^\s]+)$',
                          stdout, re.MULTILINE)
    if bindings:
        if sorted(int(rank) for rank, _, _ in bindings) != list(range(p)):
            raise ValueError(f'{path}: incomplete rank-to-device binding record')
        for _, _, devices in bindings:
            if (g == 0 and devices != '-1') or (g > 0 and len(devices.split(',')) != g):
                raise ValueError(f'{path}: rank sees wrong CUDA device count')
    elif g or p > 32:
        raise ValueError(f'{path}: missing rank-to-device binding record')
    return math.fsum(t for _, _, t in values)


def gpu_compare(reference, candidate):
    left, right = snapshot_index(reference), snapshot_index(candidate)
    if left.keys() != right.keys():
        raise ValueError('GPU physical snapshot times differ')
    report = []
    for time in sorted(left):
        with netCDF4.Dataset(left[time]) as a, netCDF4.Dataset(right[time]) as b:
            ai, ac = order(a)
            bi, bc = order(b)
            if ac != bc or set(a.variables) != set(b.variables):
                raise ValueError(f'GPU count/variable mismatch at {time}')
            arrays = {}
            for name in sorted(a.variables):
                av, bv = a.variables[name], b.variables[name]
                if av.dtype != bv.dtype or av.dimensions != bv.dimensions:
                    raise ValueError(f'GPU schema mismatch: {name}')
                for attribute in ('units', 'unit', 'positive', 'calendar'):
                    if getattr(av, attribute, None) != getattr(bv, attribute, None):
                        raise ValueError(f'GPU metadata mismatch: {name}/{attribute}')
                x, y = np.ma.asarray(av[:]), np.ma.asarray(bv[:])
                if 'particles' in av.dimensions:
                    axis = av.dimensions.index('particles')
                    x, y = np.take(x, ai, axis=axis), np.take(y, bi, axis=axis)
                if x.shape != y.shape or not np.array_equal(np.ma.getmaskarray(x), np.ma.getmaskarray(y)):
                    raise ValueError(f'GPU mask/shape mismatch: {name}')
                xv, yv = x.compressed(), y.compressed()
                if xv.dtype.kind in 'fc' and (not np.isfinite(xv).all() or not np.isfinite(yv).all()):
                    raise ValueError(f'GPU non-finite value: {name}')
                arrays[name] = (xv, yv)
                if name not in ('lat', 'lon', 'depth', 'i', 'j', 'k') and not np.array_equal(xv, yv):
                    raise ValueError(f'GPU discrete/time/state mismatch: {name}')
            lat1, lat2 = [np.radians(v) for v in arrays['lat']]
            lon1, lon2 = [np.radians(v) for v in arrays['lon']]
            term = np.sin((lat2-lat1)/2)**2 + np.cos(lat1)*np.cos(lat2)*np.sin((lon2-lon1)/2)**2
            distance = 2*6371000*np.arctan2(np.sqrt(np.clip(term, 0, 1)), np.sqrt(np.clip(1-term, 0, 1)))
            maxima = {'horizontal_m': float(np.max(distance, initial=0)),
                      'depth_m': float(np.max(np.abs(arrays['depth'][0]-arrays['depth'][1]), initial=0))}
            for name in ('i', 'j', 'k'):
                maxima[name] = float(np.max(np.abs(arrays[name][0]-arrays[name][1]), initial=0))
            if maxima['horizontal_m'] > 1e-6 or maxima['depth_m'] > 1e-8 or any(maxima[k] > 1e-8 for k in ('i', 'j', 'k')):
                raise ValueError(f'GPU numerical mismatch at {time}: {maxima}')
            report.append({'physical_time': time, 'particles': ac, 'maxima': maxima,
                           'reference_sha256': sha(left[time]), 'candidate_sha256': sha(right[time])})
    return {'comparison': 'stable ID and physical time; exact discrete/state fields; declared absolute coordinate tolerances',
            'horizontal_tolerance_m': 1e-6, 'depth_tolerance_m': 1e-8,
            'grid_index_tolerance': 1e-8, 'snapshots': report}


def gridded_compare(reference, candidate):
    left = {p.name: p for p in reference.glob('*.nc')}
    right = {p.name: p for p in candidate.glob('*.nc')}
    if not left or left.keys() != right.keys():
        raise ValueError('gridded output file set differs')
    records = []
    for name in sorted(left):
        with netCDF4.Dataset(left[name]) as a, netCDF4.Dataset(right[name]) as b:
            if set(a.variables) != set(b.variables):
                raise ValueError(f'gridded variable set differs: {name}')
            for variable in a.variables:
                av, bv = a.variables[variable], b.variables[variable]
                if av.dimensions != bv.dimensions or av.dtype != bv.dtype:
                    raise ValueError(f'gridded schema differs: {name}/{variable}')
                for attribute in ('units', 'unit', 'positive', 'calendar'):
                    if getattr(av, attribute, None) != getattr(bv, attribute, None):
                        raise ValueError(f'gridded metadata differs: {name}/{variable}/{attribute}')
                x, y = np.ma.asarray(av[:]), np.ma.asarray(bv[:])
                if x.shape != y.shape or not np.array_equal(np.ma.getmaskarray(x), np.ma.getmaskarray(y)) or not np.array_equal(x.compressed(), y.compressed()):
                    raise ValueError(f'gridded values differ: {name}/{variable}')
        records.append({'name': name, 'reference_sha256': sha(left[name]),
                        'candidate_sha256': sha(right[name]), 'variables_equal': True})
    return records


def hardware_id(run):
    cpu = (run / 'provenance/cpu.txt').read_text()
    gpu = (run / 'provenance/gpus.txt').read_text()
    fields = []
    for label in ('Model name', 'Socket(s)', 'Core(s) per socket'):
        match = re.search(rf'^{re.escape(label)}:\s*(.+)$', cpu, re.MULTILINE)
        if not match:
            raise ValueError(f'{run}: missing CPU topology field {label}')
        fields.append(match.group(1).strip())
    models = [re.sub(r'^GPU \d+: ', '', line).split(' (UUID:')[0]
              for line in gpu.splitlines() if re.match(r'^GPU \d+: ', line)]
    if not models or len(set(models)) != 1:
        raise ValueError(f'{run}: mixed or unavailable GPU node model')
    return '|'.join(fields + [models[0]])


def job_records(run):
    used = []
    superseded = []
    for repetition in (1, 2, 3):
        original = run / 'provenance' / f'job-id-{repetition}.txt'
        resumed = run / 'provenance' / f'job-id-{repetition}-resume.txt'
        selected = resumed if resumed.is_file() else original
        if not selected.is_file():
            raise ValueError(f'{run}: missing job ID for repetition {repetition}')
        used.append((repetition, selected.read_text().strip()))
        if resumed.is_file() and original.is_file():
            superseded.append((repetition, original.read_text().strip()))
    return used, superseded


def process(root):
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
    if (len(source_document.get('features', [])) != 1 or
            source_document['features'][0]['properties'].get('particlesPerHour') != particles_per_hour):
        raise ValueError('source manifest disagrees with declared emission rate')
    identity = None
    for run in sorted(root.glob('p*_n*_g*')):
        p, n, g = map(int, re.fullmatch(r'p(\d+)_n(\d+)_g(\d+)', run.name).groups())
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
    records, best, gpu_complete = performance_protocol.load(root)
    (root / 'results.json').write_text(json.dumps({'selected_cpu': best, 'gpu_sweep_complete': gpu_complete,
                                                  'records': records}, indent=2) + '\n')
    for record in records:
        performance_protocol.write_note(Path(record['source_path']).parent / 'codex-performance-review.md', record, best)
    performance_protocol.plot(root, records, best, gpu_complete)
    return best, gpu_complete


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_root', type=Path)
    args = parser.parse_args()
    try:
        best, gpu_complete = process(args.run_root)
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
    print(f'Selected CPU tuple: {best}; complete GPU sweep: {gpu_complete}')


if __name__ == '__main__':
    main()
