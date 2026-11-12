#!/usr/bin/env python3
"""Archive one native preparation conversion from a completed explicit repair."""
import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import time

from repair_angle import EXAMPLE, checksum


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--forcing-root', required=True, type=Path)
    parser.add_argument('--run-root', required=True, type=Path)
    parser.add_argument('--binary', required=True, type=Path)
    args = parser.parse_args()
    forcing = args.forcing_root.resolve(strict=True)
    record = json.loads((forcing / 'provenance/repair.json').read_text())
    config = json.loads((EXAMPLE / 'webinar-roms-usecase-download.json').read_text())
    if not record['complete'] or [entry['name'] for entry in record['files']] != config['io']['nc_inputs']:
        raise ValueError('complete repair for the exact configured file sequence required')
    binary = args.binary.resolve(strict=True)
    cache = binary.parent / 'CMakeCache.txt'
    options = cache.read_text().splitlines()
    if any(f'USE_{backend}:BOOL=OFF' not in options for backend in ('MPI', 'OMP', 'CUDA')):
        raise ValueError('a serial application build is required')
    root = args.run_root.resolve()
    root.mkdir(parents=True, exist_ok=False)
    (root / 'processed').mkdir()
    provenance = root / 'provenance'
    provenance.mkdir()
    shutil.copy2(forcing / 'provenance/repair.json', provenance / 'repair.json')
    shutil.copy2(forcing / 'provenance/policy.json', provenance / 'policy.json')
    shutil.copy2(cache, provenance / 'CMakeCache.txt')
    shutil.copy2(__file__, provenance / 'convert_repaired.py')
    shutil.copy2(binary, root / 'wacommplusplus')
    repository = EXAMPLE.parents[1]
    for filename, command in [('revision.txt', ['git', 'rev-parse', 'HEAD']),
                              ('working.patch', ['git', 'diff', 'HEAD']),
                              ('dependencies.txt', ['ldd', str(binary)])]:
        (provenance / filename).write_bytes(subprocess.check_output(command, cwd=repository))
    config['io']['base_path'] = str(forcing) + '/'
    config['io']['nc_input_root'] = str(root / 'processed/ocm3_d03_')
    config['io']['nc_output_root'] = str(root / 'output/wacomm_his_')
    configuration = root / 'wacomm.json'
    configuration.write_text(json.dumps(config, indent=2)+'\n')
    result = {'status': 'verifying_forcing', 'binary_sha256': checksum(root / 'wacommplusplus'),
              'configuration_sha256': checksum(configuration), 'platform': platform.platform(),
              'scope': 'single preparation conversion; not a performance-protocol sample',
              'random_seed': config['physics']['random_seed'], 'outputs': []}
    report = provenance / 'conversion.json'
    report.write_text(json.dumps(result, indent=2)+'\n')
    for entry in record['files']:
        if checksum(forcing / entry['name']) != entry['derived_sha256']:
            raise ValueError('derived forcing checksum mismatch: ' + entry['name'])
    result['status'] = 'converting'
    report.write_text(json.dumps(result, indent=2)+'\n')
    start = time.monotonic()
    with (root / 'run.out').open('w') as stdout, (root / 'run.err').open('w') as stderr:
        run = subprocess.run([str(root / 'wacommplusplus'), str(configuration)], cwd=root,
                             env=dict(os.environ, OMP_NUM_THREADS='1', CUDA_VISIBLE_DEVICES='-1'),
                             stdout=stdout, stderr=stderr)
    result.update(exit_code=run.returncode, elapsed_seconds=time.monotonic()-start,
                  status='failed' if run.returncode else 'converted')
    report.write_text(json.dumps(result, indent=2)+'\n')
    run.check_returncode()
    for path in sorted((root / 'processed').glob('*.nc')):
        result['outputs'].append({'name': path.name, 'bytes': path.stat().st_size, 'sha256': checksum(path)})
    if len(result['outputs']) != len(config['io']['nc_inputs']):
        result['status'] = 'incomplete_output'
    report.write_text(json.dumps(result, indent=2)+'\n')
    if result['status'] != 'converted':
        raise ValueError('incomplete native output sequence')
    print(report)


if __name__ == '__main__':
    main()
