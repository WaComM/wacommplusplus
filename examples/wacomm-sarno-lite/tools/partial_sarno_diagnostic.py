#!/usr/bin/env python3
"""Archive a provisional Sarno CPU diagnostic while two-node runs are pending."""
import argparse
import json
from pathlib import Path
from statistics import median

from sarno_protocol_collect import compare, gridded_compare, hardware_id, sha, solver_seconds


def collect(root, candidate):
    reference_run = root / 'p1_n1_g0'
    candidate_run = root / f'p{candidate[0]}_n{candidate[1]}_g0'
    reference = reference_run / 'sample-1'
    if not reference.is_dir() or not candidate_run.is_dir():
        raise ValueError('baseline or candidate run is absent')
    records = []
    for run in sorted(root.glob('p*_n*_g0')):
        p, n, g = map(int, run.name[1:].replace('_n', ' ').replace('_g', ' ').split())
        if not (run / 'sample-3/exit.txt').is_file():
            continue
        if any((run / f'sample-{rep}/exit.txt').read_text().strip() != '0' for rep in (1, 2, 3)):
            raise ValueError(f'{run}: unsuccessful measured sample')
        samples = [solver_seconds(run / f'sample-{rep}/run.out', p, n, g)
                   for rep in (1, 2, 3)]
        records.append({'mpi_processes': p, 'openmp_threads': n,
                        'solver_seconds': samples, 'median_seconds': median(samples),
                        'scientific_equivalence': 'not checked by this diagnostic'})
    by_key = {(r['mpi_processes'], r['openmp_threads']): r for r in records}
    if (1, 1) not in by_key or candidate not in by_key:
        raise ValueError('baseline or candidate is incomplete')
    baseline_binary = sha(reference_run / 'wacommplusplus')
    if sha(candidate_run / 'wacommplusplus') != baseline_binary:
        raise ValueError('baseline and candidate executables differ')
    baseline_hardware = hardware_id(reference_run)
    if hardware_id(candidate_run) != baseline_hardware:
        raise ValueError('baseline and candidate hardware classes differ')
    config = 'wacomm-sarno-lite-6h.json'
    source = 'examples/sources-sarno_river/sources-sarno_river.json'
    input_hashes = {'binary_sha256': baseline_binary,
                    'configuration_sha256': sha(reference / config),
                    'source_sha256': sha(reference / source)}
    comparisons = []
    for rep in (1, 2, 3):
        sample = candidate_run / f'sample-{rep}'
        if sha(sample / config) != input_hashes['configuration_sha256'] or sha(sample / source) != input_hashes['source_sha256']:
            raise ValueError(f'{sample}: scientific inputs differ from baseline')
        particle = compare(reference / 'snapshots-6h', sample / 'snapshots-6h')
        grid = gridded_compare(reference / 'output-6h', sample / 'output-6h')
        comparisons.append({'repetition': rep, 'particle_comparison': particle,
                            'gridded_comparison': grid})
    by_key[candidate]['scientific_equivalence'] = 'exact particle and gridded comparison passed for all three repetitions'
    best = min(records, key=lambda r: (r['median_seconds'], r['mpi_processes'], r['openmp_threads']))
    return {'schema': 'wacomm-sarno-partial-diagnostic-v1',
            'suite': str(root),
            'problem_size_particles_per_hour': int((root / 'provenance/particles-per-hour.txt').read_text()),
            'revision': (root / 'provenance/revision.txt').read_text().strip(),
            'hardware_id': baseline_hardware,
            'status': 'incomplete CPU matrix; no GPU selection or final recommendation',
            'completed_cpu_tuples': len(records),
            'best_completed_timing_tuple': (best['mpi_processes'], best['openmp_threads'], 0),
            'candidate': (*candidate, 0), 'input_hashes': input_hashes,
            'records': records, 'candidate_comparisons': comparisons}


def write_notes(root, report):
    output = root / 'provenance/partial-one-node-diagnostic.json'
    candidate = report['candidate']
    note = root / 'provenance/partial-one-node-diagnostic.md'
    note.write_text(
        f"# Provisional Sarno performance diagnostic: {report['problem_size_particles_per_hour']} particles/hour\n\n"
        f"Only {report['completed_cpu_tuples']} CPU resource tuples are complete; the required 64-rank/two-node tuple is pending. "
        'Do not publish a complete speedup curve, select a final CPU tuple, or schedule the formal GPU sweep from this diagnostic.\n\n'
        f"- Candidate: {candidate[0]}/{candidate[1]}/0; its three particle and gridded output comparisons passed exactly.\n"
        f"- Measured one-node timing records and comparison hashes: `{output.name}`.\n"
        '- Analyze any proposed change in the common solver and rerun backend-equivalence, forward/backward, restart, and same-hardware performance checks.\n')
    for record in report['records']:
        p, n = record['mpi_processes'], record['openmp_threads']
        run_note = root / f'p{p}_n{n}_g0/codex-performance-review.md'
        run_note.write_text(
            f'# Provisional Sarno performance review: {p}/{n}/0\n\n'
            f"This {report['problem_size_particles_per_hour']}-particles/hour timing is part of an incomplete CPU matrix. "
            'Do not use it for a final speedup chart, CPU selection, or GPU selection until the 64-rank/two-node samples and full collector validation pass.\n\n'
            f"- Revision: `{report['revision']}`\n"
            f"- Hardware class: `{report['hardware_id']}`\n"
            f"- Three solver-duration sums (s): `{record['solver_seconds']}`\n"
            f"- Median solver duration (s): `{record['median_seconds']}`\n"
            f"- Scientific equivalence in this partial check: {record['scientific_equivalence']}\n"
            f"- Candidate comparison and shared input hashes: `../provenance/{output.name}`\n\n"
            'Analyze a suspected performance defect in the common solver and execution hierarchy. '
            'Require a generic fix, backend equivalence, forward/backward and restart checks, and same-hardware before/after timings. '
            'Do not change equations, scientific settings, random seeds, or tolerances to improve this one example.\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('suite', type=Path)
    parser.add_argument('--candidate', required=True, help='MPI-processes:OpenMP-threads')
    args = parser.parse_args()
    candidate = tuple(map(int, args.candidate.split(':')))
    if len(candidate) != 2 or min(candidate) <= 0:
        raise ValueError('candidate must contain two positive resource counts')
    report = collect(args.suite, candidate)
    output = args.suite / 'provenance/partial-one-node-diagnostic.json'
    output.write_text(json.dumps(report, indent=2) + '\n')
    write_notes(args.suite, report)
    print(output)


if __name__ == '__main__':
    main()
