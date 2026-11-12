#!/usr/bin/env python3
"""Compare fully validated resource sweeps across declared problem sizes."""
import argparse
import json
import math
from pathlib import Path

import performance_protocol


def collect(cases):
    if not cases:
        raise ValueError('at least one workload sweep is required')
    if len({size for size, _ in cases}) != len(cases):
        raise ValueError('duplicate problem size')
    summaries = []
    common = None
    unit = None
    for size, root in sorted(cases):
        if not math.isfinite(size) or size <= 0:
            raise ValueError('problem sizes must be positive and finite')
        records, best, gpu_reference, gpu_complete = performance_protocol.load(root)
        if not gpu_complete:
            raise ValueError(f'{root}: complete GPU sweep required')
        if any(r.get('problem_size') != size for r in records):
            raise ValueError(f'{root}: problem-size metadata differs from declared size')
        units = {r.get('problem_size_unit') for r in records}
        if len(units) != 1 or not next(iter(units)):
            raise ValueError(f'{root}: inconsistent or missing problem-size unit')
        current_unit = next(iter(units))
        if unit is None:
            unit = current_unit
        elif current_unit != unit:
            raise ValueError('problem-size units differ')
        fields = ('configuration_sha256', 'forcing_sha256',
                  'binary_sha256', 'hardware_id', 'timing_scope')
        signature = tuple(records[0][field] for field in fields)
        if common is None:
            common = signature
        elif common != signature:
            raise ValueError(f'{root}: build, forcing, configuration, or hardware differs')
        sources = {r.get('source_sha256') for r in records}
        if len(sources) != 1 or not next(iter(sources)):
            raise ValueError(f'{root}: source hashes missing or inconsistent')
        by_key = {performance_protocol.key(r): r for r in records}
        cpu = by_key[best]
        gpu_cpu = by_key[gpu_reference]
        p, n, _ = gpu_reference
        values = []
        for g in range(5):
            r = by_key[(p, n, g)]
            values.append({'gpu_devices': g, 'solver_seconds': r['solver_seconds'],
                           'median_seconds': r['median_seconds'],
                           'application_seconds': r.get('application_seconds'),
                           'incremental_speedup': gpu_cpu['median_seconds'] / r['median_seconds'],
                           'gpu_device_speedup': r['gpu_device_speedup'],
                           'gpu_device_efficiency': r['gpu_device_efficiency']})
        best_measured = min([cpu] + [by_key[(p, n, g)] for g in range(1, 5)],
                            key=lambda r: (r['median_seconds'], performance_protocol.key(r)))
        summaries.append({'problem_size': size, 'problem_size_unit': unit,
                          'run_root': str(root), 'revision': records[0]['revision'],
                          'source_sha256': next(iter(sources)),
                          'selected_cpu': best,
                          'gpu_reference_cpu': gpu_reference,
                          'serial_median_seconds': by_key[(1, 1, 0)]['median_seconds'],
                          'selected_cpu_median_seconds': cpu['median_seconds'],
                          'selected_cpu_speedup': cpu['speedup'],
                          'selected_cpu_efficiency': cpu['cpu_efficiency'],
                          'best_measured_configuration': performance_protocol.key(best_measured),
                          'best_measured_median_seconds': best_measured['median_seconds'],
                          'gpu_sweep': values})
    return {'schema': 'wacomm-performance-workload-scaling-v1',
            'common_provenance': dict(zip(fields, common)), 'problem_size_unit': unit,
            'cases': summaries}


def plot(summary, output):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    cases = summary['cases']
    x = [c['problem_size'] for c in cases]
    unit = summary['problem_size_unit']
    fig, ax = plt.subplots()
    ax.plot(x, [c['serial_median_seconds'] for c in cases], marker='o', label='1/1/0')
    ax.plot(x, [c['selected_cpu_median_seconds'] for c in cases], marker='o', label='selected CPU')
    for g in range(1, 5):
        ax.plot(x, [c['gpu_sweep'][g]['median_seconds'] for c in cases], marker='o', label=f'selected CPU + {g} GPU')
    ax.set(xscale='log', yscale='log', xlabel=f'Problem size ({unit})', ylabel='Median solver time (s)')
    ax.grid(True, which='both')
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(output / 'workload-runtime.svg')
    plt.close(fig)
    fig, ax = plt.subplots()
    for g in range(1, 5):
        ax.plot(x, [c['gpu_sweep'][g]['incremental_speedup'] for c in cases], marker='o', label=f'{g} GPU')
    ax.axhline(1, linestyle=':', color='gray', label='selected CPU parity')
    ax.set(xscale='log', xlabel=f'Problem size ({unit})', ylabel='GPU speedup over selected CPU')
    ax.grid(True, which='both')
    ax.legend()
    fig.tight_layout()
    fig.savefig(output / 'workload-gpu-speedup.svg')
    plt.close(fig)
    fig, ax = plt.subplots()
    ax.plot(x, [c['selected_cpu_speedup'] for c in cases], marker='o', label='CPU speedup')
    ax.plot(x, [c['selected_cpu_efficiency'] for c in cases], marker='s', label='CPU efficiency')
    ax.set(xscale='log', xlabel=f'Problem size ({unit})', ylabel='Dimensionless ratio')
    ax.grid(True, which='both')
    ax.legend()
    fig.tight_layout()
    fig.savefig(output / 'workload-cpu-selection.svg')
    plt.close(fig)
    for name in ('workload-runtime.svg', 'workload-gpu-speedup.svg',
                 'workload-cpu-selection.svg'):
        path = output / name
        path.write_text('\n'.join(line.rstrip(' \t') for line in path.read_text().splitlines()) + '\n')


def write_report(summary, output):
    lines = [
        '# Measured workload-dependent resource choices',
        '',
        'Each row reports the fastest validated CPU tuple at that emission rate. The GPU comparison uses the listed one-node CPU reference with zero through four devices at fixed MPI/OpenMP placement. Times are medians of the independent solver-duration sums; these are observed choices on the recorded hardware, not universal optima.',
        '',
        '| Particles/hour | Selected CPU (p/n/0) | GPU reference (p/n/0) | CPU median (s) | CPU speedup | CPU efficiency | Best measured (p/n/g) | Best median (s) |',
        '| ---: | :---: | :---: | ---: | ---: | ---: | :---: | ---: |',
    ]
    for case in summary['cases']:
        cpu = '/'.join(map(str, case['selected_cpu']))
        gpu_reference = '/'.join(map(str, case['gpu_reference_cpu']))
        best = '/'.join(map(str, case['best_measured_configuration']))
        lines.append(f"| {case['problem_size']:g} | {cpu} | {gpu_reference} | {case['selected_cpu_median_seconds']:.6g} | {case['selected_cpu_speedup']:.3f} | {case['selected_cpu_efficiency']:.3f} | {best} | {case['best_measured_median_seconds']:.6g} |")
    lines += ['', 'A device count is favored only when its measured median is lower at the same emission rate; differences from three repetitions are descriptive and do not establish statistical significance. Examine the per-case speedup and efficiency charts, raw samples, validation reports, and device telemetry before attributing a bottleneck or generalizing to other hardware.', '']
    (output / 'workload-summary.md').write_text('\n'.join(lines))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case', action='append', required=True,
                        help='problem-size:path-to-validated-resource-sweep; repeat per size')
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    cases = []
    for item in args.case:
        size, path = item.split(':', 1)
        cases.append((int(size), Path(path)))
    summary = collect(cases)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / 'workload-results.json').write_text(json.dumps(summary, indent=2) + '\n')
    plot(summary, args.output_dir)
    write_report(summary, args.output_dir)
    (args.output_dir / 'codex-workload-review.md').write_text(
        '# Cross-workload WaComM++ performance review\n\n'
        'Analyze the validated size sweeps in `workload-results.json` and their per-run Codex notes. '
        'Identify generic scaling bottlenecks and resource thresholds without changing the scientific workload, '
        'equations, forcing, random seed, or numerical tolerances. Distinguish measured results from hypotheses. '
        'Require forward/backward, restart, MPI/OpenMP/CUDA equivalence, and before/after same-hardware tests '
        'for any proposed core improvement.\n')


if __name__ == '__main__':
    main()
