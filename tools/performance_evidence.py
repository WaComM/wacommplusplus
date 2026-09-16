#!/usr/bin/env python3
"""Read-only numerical and runtime checks for declared performance workloads."""
import hashlib
import math
import re
import netCDF4
import numpy as np
from compare_particle_snapshots import compare, snapshot_index, order

def sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def solver_seconds(path, p, n, g, intervals):
    stdout = path.read_text()
    matches = re.findall(r'Solver interval: start=([^ ]+) end=([^ ]+) seconds=([^\s]+)', stdout)
    values = [(float(a), float(b), float(c)) for a, b, c in matches]
    if [(a, b) for a, b, _ in values] != list(zip(intervals[:-1], intervals[1:])):
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
