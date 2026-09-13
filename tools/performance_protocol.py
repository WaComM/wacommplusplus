#!/usr/bin/env python3
"""Validate and summarize the shared strong-scaling experiment."""
import argparse
import hashlib
import json
import math
from pathlib import Path
from statistics import median

MPI = [(p, 1, 0) for p in (1, 2, 4, 8, 16, 32, 64)]
OMP = [(1, n, 0) for n in (1, 2, 4, 8, 16, 32)]
HYBRID = [(p, 32 // p, 0) for p in (1, 2, 4, 8, 16, 32)]
CPU = set(MPI + OMP + HYBRID)


def key(record):
    return tuple(record[k] for k in ('mpi_processes', 'openmp_threads', 'gpu_devices'))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(root):
    records = []
    for path in sorted(root.glob('p*_n*_g*/run.json')):
        r = json.loads(path.read_text())
        k = key(r)
        if path.parent.name != f'p{k[0]}_n{k[1]}_g{k[2]}':
            raise ValueError(f'{path}: directory and resource counts disagree')
        if type(r.get('exit_code')) is not int or r['exit_code'] != 0 or r.get('equivalence_passed') is not True:
            raise ValueError(f'{path}: failed execution or scientific equivalence')
        samples = r.get('solver_seconds')
        if not isinstance(samples, list) or len(samples) < 3 or any(type(t) not in (int, float) or not math.isfinite(t) or t <= 0 for t in samples):
            raise ValueError(f'{path}: at least three positive finite independent run times required')
        for field in ('revision', 'configuration_sha256', 'forcing_sha256', 'binary_sha256', 'hardware_id', 'timing_scope', 'validation_report'):
            if not isinstance(r.get(field), str) or not r[field]:
                raise ValueError(f'{path}: missing {field}')
        if r['timing_scope'] != 'solver':
            raise ValueError(f'{path}: inconsistent timing scope')
        report = path.parent / r['validation_report']
        if report.resolve().parent != path.parent.resolve() or not report.is_file():
            raise ValueError(f'{path}: missing local validation report')
        r['median_seconds'] = median(samples)
        r['run_json_sha256'] = digest(path)
        r['source_path'] = str(path)
        records.append(r)
    by_key = {}
    for r in records:
        k = key(r)
        if k in by_key:
            raise ValueError(f'duplicate configuration {k}')
        by_key[k] = r
    if not CPU <= by_key.keys():
        raise ValueError(f'missing CPU configurations: {sorted(CPU - by_key.keys())}')
    baseline = by_key[(1, 1, 0)]
    identity = ('revision', 'configuration_sha256', 'forcing_sha256', 'binary_sha256', 'hardware_id', 'timing_scope')
    for r in records:
        if any(r[f] != baseline[f] for f in identity):
            raise ValueError(f'{r["source_path"]}: workload/build/hardware mismatch')
    best = min((by_key[k] for k in CPU), key=lambda r: (r['median_seconds'], key(r)))
    p, n, _ = key(best)
    gpu = {(p, n, g) for g in range(5)}
    if by_key.keys() - CPU - gpu:
        raise ValueError('unexpected GPU configuration')
    if any(k[2] for k in by_key) and not gpu <= by_key.keys():
        raise ValueError(f'incomplete GPU sweep: {sorted(gpu - by_key.keys())}')
    gpu_one = by_key.get((p, n, 1))
    for r in records:
        r['gpu_device_speedup'] = gpu_one['median_seconds'] / r['median_seconds'] if gpu_one and r['gpu_devices'] else None
        r['gpu_device_efficiency'] = r['gpu_device_speedup'] / r['gpu_devices'] if r['gpu_device_speedup'] is not None else None
        r['speedup'] = baseline['median_seconds'] / r['median_seconds']
        r['cpu_efficiency'] = r['speedup'] / (r['mpi_processes'] * r['openmp_threads']) if r['gpu_devices'] == 0 else None
        r['gpu_incremental_speedup'] = best['median_seconds'] / r['median_seconds'] if r['gpu_devices'] else None
    return records, key(best), bool(gpu <= by_key.keys())


def write_note(path, record, best):
    k = key(record)
    path.write_text(f'''# Performance review: {k[0]}/{k[1]}/{k[2]}

Analyze the WaComM++ core and this example using the evidence below. Diagnose anomalies and any failed validation before proposing changes. Trace bottlenecks through the common solver and execution hierarchy; make fixes generic across examples, tracking directions, restarts, and supported backends. Preserve the governing equations, units, deterministic seed behavior, and documented tolerances. Add relevant backend-equivalence and regression tests and synchronize model, configuration, example, and reference documentation for any user-visible change. Do not infer observational validity from timing agreement.

- Run record: `{record['source_path']}` (SHA-256 `{record['run_json_sha256']}`)
- Validation evidence: `{record['validation_report']}`; equivalence passed
- Revision: `{record['revision']}`
- Configuration SHA-256: `{record['configuration_sha256']}`
- Forcing SHA-256: `{record['forcing_sha256']}`
- Binary SHA-256: `{record['binary_sha256']}`
- Hardware ID: `{record['hardware_id']}`
- Solver samples (s): `{record['solver_seconds']}`
- Median solver time (s): `{record['median_seconds']:.9g}`
- Baseline-relative speedup: `{record['speedup']:.6g}`
- CPU efficiency: `{record['cpu_efficiency'] if record['cpu_efficiency'] is not None else 'not defined for heterogeneous resources'}`
- GPU device speedup/efficiency (relative to one GPU): `{record['gpu_device_speedup']}` / `{record['gpu_device_efficiency']}`
- Selected CPU configuration: `{best[0]}/{best[1]}/0`

Report reproducible evidence, a root-cause hypothesis, a scoped generic patch, scientific/backend/restart checks, and before/after measurements on the same workload and hardware. Treat this note as a review prompt, not authorization to alter scientific behavior without validation.
''')


def plot(root, records, best, gpu_complete):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    groups = [('MPI', MPI), ('OpenMP', OMP), ('hybrid', HYBRID)]
    by_key = {key(r): r for r in records}
    for metric, ylabel in [('speedup', 'Speedup'), ('cpu_efficiency', 'CPU efficiency')]:
        fig, ax = plt.subplots()
        for label, keys in groups:
            ax.plot(range(len(keys)), [by_key[k][metric] for k in keys], marker='o', label=label)
            ideal = [k[0] * k[1] if metric == 'speedup' else 1 for k in keys]
            ax.plot(range(len(keys)), ideal, linestyle=':', alpha=0.5, label=f'{label} ideal')
        ax.set_xticks(range(7))
        ax.set_xlabel('Sweep point index (see results.json for resource counts)')
        ax.set_ylabel(ylabel)
        ax.grid(True)
        ax.legend()
        fig.tight_layout()
        fig.savefig(root / f'{metric}.svg')
        plt.close(fig)
    if gpu_complete:
        fig, ax = plt.subplots()
        p, n, _ = best
        ax.plot(range(5), [by_key[(p, n, g)]['gpu_incremental_speedup'] for g in range(5)], marker='o')
        ax.set(xlabel='GPU devices', ylabel='Speedup relative to selected CPU configuration')
        ax.set_xticks(range(5))
        ax.grid(True)
        fig.tight_layout()
        fig.savefig(root / 'gpu_incremental_speedup.svg')
        plt.close(fig)
        fig, ax = plt.subplots()
        for metric, label in [('gpu_device_speedup', 'GPU speedup'), ('gpu_device_efficiency', 'GPU efficiency')]:
            ax.plot(range(1, 5), [by_key[(p, n, g)][metric] for g in range(1, 5)], marker='o', label=label)
        ax.set(xlabel='GPU devices', ylabel='Dimensionless ratio')
        ax.set_xticks(range(1, 5))
        ax.grid(True)
        ax.legend()
        fig.tight_layout()
        fig.savefig(root / 'gpu_device_scaling.svg')
        plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_root', type=Path)
    args = parser.parse_args()
    records, best, gpu_complete = load(args.run_root)
    summary = {'selected_cpu': best, 'gpu_sweep_complete': gpu_complete, 'records': records}
    (args.run_root / 'results.json').write_text(json.dumps(summary, indent=2) + '\n')
    for r in records:
        write_note(Path(r['source_path']).parent / 'codex-performance-review.md', r, best)
    plot(args.run_root, records, best, gpu_complete)


if __name__ == '__main__':
    main()
