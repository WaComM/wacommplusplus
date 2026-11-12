#!/usr/bin/env python3
"""Archive and time the webinar ROMS dry conversion without changing the solver."""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import resource
import shutil
import signal
import statistics
import subprocess
import time
from datetime import datetime, timezone

EXAMPLE = Path(__file__).resolve().parents[1]
REPOSITORY = EXAMPLE.parents[1]


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def capture(command):
    try:
        result = subprocess.run(command, universal_newlines=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, cwd=REPOSITORY)
        return result.stdout
    except OSError as error:
        return 'Unavailable: ' + str(error) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--run-root', type=Path, required=True)
    parser.add_argument('--base-path', help='Explicit local mirror or remote URL; no automatic fallback')
    parser.add_argument('--repetitions', type=int, default=3)
    parser.add_argument('--timeout', type=float, default=600)
    args = parser.parse_args()
    if args.repetitions < 3 or not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error('at least three repetitions and a positive finite timeout are required')
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    binary = args.binary.resolve(strict=True)
    ncdump = shutil.which('ncdump')
    if not ncdump:
        parser.error('ncdump is required for read-only output comparison')
    config_path = EXAMPLE / 'webinar-roms-usecase-download.json'
    config = json.loads(config_path.read_text())
    if args.base_path:
        base = args.base_path
        if '://' not in base:
            base = str(Path(base).resolve(strict=True))
        config['io']['base_path'] = base.rstrip('/') + '/'
    root = args.run_root.resolve()
    root.mkdir(parents=True, exist_ok=False)
    base = config['io']['base_path']
    manifest = []
    for name in config['io']['nc_inputs']:
        location = base + name
        manifest.append({'location': location, 'sha256': None if '://' in location
                         else digest(Path(location))})
    env = dict(os.environ, OMP_NUM_THREADS='1', OMP_DYNAMIC='FALSE')
    metadata = {
        'revision': capture(['git', 'rev-parse', 'HEAD']).strip(),
        'utc': datetime.now(timezone.utc).isoformat(),
        'timeout_seconds': args.timeout, 'measured_repetitions': args.repetitions,
        'platform': platform.platform(), 'hostname': platform.node(),
        'cpu_affinity': sorted(os.sched_getaffinity(0)) if hasattr(os, 'sched_getaffinity') else None,
        'binary': str(binary), 'binary_sha256': digest(binary),
        'runner_sha256': digest(Path(__file__)), 'configuration_sha256': digest(config_path),
        'forcing': manifest, 'random_seed': config['physics']['random_seed'],
        'restart': 'disabled', 'sources': 'disabled',
        'environment': {key: env.get(key) for key in ('OMP_NUM_THREADS', 'OMP_DYNAMIC',
                         'OMP_PROC_BIND', 'OMP_PLACES', 'SLURM_JOB_ID')},
        'timing_scope': 'application wall seconds: process startup, forcing reads, normalization, writes, shutdown',
        'resources': 'one directly launched process; OMP_NUM_THREADS=1; no particle solver',
        'tolerance': 'exact ncdump -p 9,17 values and metadata across repetitions; not observational validation',
        'samples': [], 'status': 'incomplete',
    }
    (root / 'working.patch').write_text(capture(['git', 'diff', 'HEAD']))
    shutil.copy2(config_path, root / 'original.json')
    shutil.copy2(__file__, root / 'runner.py')
    cache = binary.parent / 'CMakeCache.txt'
    if cache.exists():
        shutil.copy2(cache, root / 'CMakeCache.txt')
    (root / 'dependencies.txt').write_text(
        'System nc-config probe; consult CMakeCache.txt for the linked build.\n'
        + capture(['nc-config', '--all']))
    (root / 'platform.txt').write_text(capture(['uname', '-a']))
    write_json(root / 'run.json', metadata)
    reference = None
    for index in range(args.repetitions + 1):
        directory = root / ('warmup' if index == 0 else f'repeat-{index}')
        directory.mkdir()
        config['io']['nc_input_root'] = str(directory / 'ocm3_d03_')
        config['io']['nc_output_root'] = str(directory / 'unused_')
        write_json(directory / 'configuration.json', config)
        command = [str(binary), str(directory / 'configuration.json')]
        start = time.perf_counter()
        with (directory / 'stdout.log').open('w') as stdout, (directory / 'stderr.log').open('w') as stderr:
            process = subprocess.Popen(command, cwd=directory, env=env, stdout=stdout,
                                       stderr=stderr, start_new_session=True)
            timed_out = False
            try:
                code = process.wait(timeout=args.timeout)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                code = process.wait()
                timed_out = True
        sample = {'order': index, 'warmup': index == 0, 'command': command,
                  'elapsed_seconds': time.perf_counter() - start, 'exit_code': code,
                  'timed_out': timed_out, 'outputs': []}
        metadata['samples'].append(sample)
        try:
            if code or timed_out:
                raise ValueError('application failed or timed out; see archived logs')
            outputs = sorted(directory.glob('ocm3_d03_*.nc'))
            if len(outputs) != len(manifest):
                raise ValueError(f'expected {len(manifest)} converted files, found {len(outputs)}')
            values = {}
            for output in outputs:
                dump = directory / (output.stem + '.cdl')
                with dump.open('w') as stream:
                    subprocess.run([ncdump, '-n', 'converted', '-p', '9,17', str(output)],
                                   stdout=stream, check=True, timeout=args.timeout)
                values[output.name] = digest(dump)
                sample['outputs'].append({'name': output.name, 'sha256': digest(output),
                                          'values_sha256': values[output.name]})
            if reference is None:
                reference = values
            elif values != reference:
                raise ValueError('converted values or metadata differ from warmup')
            for entry in manifest:
                if entry['sha256'] is not None and digest(Path(entry['location'])) != entry['sha256']:
                    raise ValueError('local forcing changed during the suite')
            sample['comparison_passed'] = True
        except (OSError, ValueError, subprocess.SubprocessError) as error:
            metadata['status'] = 'failed'
            sample['error'] = str(error)
            write_json(root / 'run.json', metadata)
            print(f'Failed; evidence: {root / "run.json"}')
            return 1
        write_json(root / 'run.json', metadata)
    elapsed = [sample['elapsed_seconds'] for sample in metadata['samples'][1:]]
    metadata.update(status='complete', median_seconds=statistics.median(elapsed),
                    range_seconds=[min(elapsed), max(elapsed)])
    write_json(root / 'run.json', metadata)
    print(f'Conversion wall time median {metadata["median_seconds"]:.6f} s; evidence: {root}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
