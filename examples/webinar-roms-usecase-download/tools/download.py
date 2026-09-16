#!/usr/bin/env python3
"""Download the explicitly configured ROMS files into a checksummed local mirror."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import threading

EXAMPLE = Path(__file__).resolve().parents[1]


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--destination', type=Path, required=True)
    parser.add_argument('--base-url', default='https://data.meteo.uniparthenope.it/files/rms3/d03/history/')
    parser.add_argument('--workers', type=int, default=4)
    args = parser.parse_args()
    if args.workers < 1 or args.workers > 4:
        parser.error('workers must be between one and four')
    config = json.loads((EXAMPLE / 'webinar-roms-usecase-download.json').read_text())
    root = args.destination.resolve()
    root.mkdir(parents=True, exist_ok=True)
    provenance = root / 'provenance'
    provenance.mkdir(exist_ok=True)
    manifest = provenance / 'download.json'
    record = json.loads(manifest.read_text()) if manifest.exists() else {
        'base_url': args.base_url, 'started_utc': datetime.now(timezone.utc).isoformat(), 'files': {}}
    if record['base_url'] != args.base_url:
        raise ValueError('cannot mix archive URLs in one mirror')
    lock = threading.Lock()

    def save():
        temporary = manifest.with_suffix('.tmp')
        temporary.write_text(json.dumps(record, indent=2) + '\n')
        temporary.replace(manifest)

    def download(name):
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        url = args.base_url.rstrip('/') + '/' + name
        if path.exists():
            prior = record['files'].get(name, {})
            if prior.get('sha256') != digest(path):
                raise ValueError(f'{name}: existing file has no matching checksum')
            print(f'Verified existing {name}', flush=True)
            return
        partial = path.with_suffix('.nc.part')
        headers = provenance / (path.name + '.headers')
        log = provenance / (path.name + '.log')
        with log.open('a') as stderr:
            subprocess.run(['curl', '--fail', '--silent', '--show-error', '--location',
                            '--retry', '3', '--connect-timeout', '30', '--max-time', '7200',
                            '--speed-time', '120', '--speed-limit', '1024', '--continue-at', '-',
                            '--dump-header', str(headers), '--output', str(partial), url],
                           stderr=stderr, check=True)
        metadata = subprocess.check_output(['ncdump', '-h', str(partial)], universal_newlines=True)
        (provenance / (path.name + '.metadata.txt')).write_text(metadata)
        times = subprocess.check_output(['ncdump', '-v', 'ocean_time', str(partial)], universal_newlines=True)
        (provenance / (path.name + '.time.txt')).write_text(times)
        checksum = digest(partial)
        entry = {'url': url, 'bytes': partial.stat().st_size, 'sha256': checksum,
                 'completed_utc': datetime.now(timezone.utc).isoformat()}
        with lock:
            record['files'][name] = entry
            save()
            partial.rename(path)
        print(f'Downloaded {name}: {entry["bytes"]} bytes, SHA-256 {checksum}', flush=True)

    failures = []
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        tasks = {pool.submit(download, name): name for name in config['io']['nc_inputs']}
        for task in as_completed(tasks):
            try:
                task.result()
            except Exception as error:
                failures.append({'name': tasks[task], 'error': str(error)})
                print(f'Failed {tasks[task]}: {error}', flush=True)
    record['failures'] = failures
    record['complete'] = not failures and all((root / name).is_file() for name in config['io']['nc_inputs'])
    save()
    if record['complete']:
        (provenance / 'forcing.sha256').write_text(''.join(
            f'{record["files"][name]["sha256"]}  {name}\n' for name in config['io']['nc_inputs']))
    print(f'Complete: {record["complete"]}; manifest: {manifest}', flush=True)
    return 0 if record['complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
