#!/usr/bin/env python3
"""Exercise isolated conversion, exact repeatability, and failure archiving."""

import datetime
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

EXAMPLE = Path(__file__).resolve().parents[1]


def main():
    if not shutil.which('ncgen') or not shutil.which('ncdump'):
        return 77
    config = json.loads((EXAMPLE / 'webinar-roms-usecase-download.json').read_text())
    assert config['simulation']['dry'] and config['io']['save_input']
    assert not config['sources']['active'] and not config['restart']['active']
    with tempfile.TemporaryDirectory(prefix='workflow-test-', dir=EXAMPLE / 'data') as temporary:
        root = Path(temporary)
        forcing = root / 'forcing'
        epoch = datetime.datetime(1968, 5, 23)
        start = int((datetime.datetime(2019, 4, 1, 8) - epoch).total_seconds())
        for index, name in enumerate(config['io']['nc_inputs']):
            output = forcing / name
            output.parent.mkdir(parents=True, exist_ok=True)
            cdl = root / 'fixture.cdl'
            cdl.write_text('''netcdf fixture {
 dimensions: ocean_time=1; s_rho=2; s_w=3;
 eta_rho=2; xi_rho=3; eta_u=2; xi_u=2; eta_v=1; xi_v=3;
 variables:
 double ocean_time(ocean_time); ocean_time:units="seconds since 1968-05-23 00:00:00 GMT";
 ocean_time:calendar="gregorian";
 double s_rho(s_rho); double s_w(s_w);
 double mask_rho(eta_rho,xi_rho); double mask_u(eta_u,xi_u); double mask_v(eta_v,xi_v);
 double lat_rho(eta_rho,xi_rho); double lon_rho(eta_rho,xi_rho);
 double lat_v(eta_v,xi_v); double lon_u(eta_u,xi_u); double h(eta_rho,xi_rho);
 float zeta(ocean_time,eta_rho,xi_rho);
 float u(ocean_time,s_rho,eta_u,xi_u); float v(ocean_time,s_rho,eta_v,xi_v);
 float w(ocean_time,s_w,eta_rho,xi_rho); float AKt(ocean_time,s_w,eta_rho,xi_rho);
 data:
 ocean_time=TIME; s_rho=-0.75,-0.25; s_w=-1,-0.5,0;
 mask_rho=1,1,1,1,1,1; mask_u=1,1,1,1; mask_v=1,1,1;
 lat_rho=40,40,40,41,41,41; lon_rho=10,11,12,10,11,12;
 lat_v=40.5,40.5,40.5; lon_u=10.5,11.5,10.5,11.5; h=100,100,100,100,100,100;
 zeta=0,0,0,0,0,0; u=2,2,2,2,2,2,2,2; v=6,6,6,6,6,6;
 w=0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0;
 AKt=0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0;
}'''.replace('TIME', str(start + index * 3600)))
            subprocess.run(['ncgen', '-o', str(output), str(cdl)], check=True)
        command = [sys.executable, str(EXAMPLE / 'tools/run.py'), '--binary', sys.argv[1],
                   '--base-path', str(forcing), '--timeout', '30', '--run-root', str(root / 'success')]
        subprocess.run(command, check=True)
        report = json.loads((root / 'success/run.json').read_text())
        assert report['status'] == 'complete' and len(report['samples']) == 4
        assert all(entry['sha256'] for entry in report['forcing'])
        for index, name in enumerate(config['io']['nc_inputs']):
            stamp = name.rsplit('_', 1)[1].replace('00.nc', '.nc')
            output = root / 'success/warmup' / ('ocm3_d03_' + stamp)
            result = subprocess.check_output(['ncdump', '-v', 'ocean_time', str(output)], universal_newlines=True)
            expected = str(start + index * 3600)
            assert expected in result
            if index < 12:
                assert str(start + (index + 1) * 3600) in result
        # An existing archive must never be overwritten.
        assert subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode != 0
        (forcing / config['io']['nc_inputs'][0]).write_bytes(b'invalid netcdf')
        command[-1] = str(root / 'failure')
        assert subprocess.run(command).returncode == 1
        failed = json.loads((root / 'failure/run.json').read_text())
        assert failed['status'] == 'failed' and 'median_seconds' not in failed
        sleeper = root / 'sleep.sh'
        sleeper.write_text('#!/bin/sh\nsleep 5\n')
        sleeper.chmod(0o700)
        command[command.index('--binary') + 1] = str(sleeper)
        command[command.index('--timeout') + 1] = '0.1'
        command[-1] = str(root / 'timeout')
        assert subprocess.run(command).returncode == 1
        timed = json.loads((root / 'timeout/run.json').read_text())
        assert timed['status'] == 'failed' and timed['samples'][0]['timed_out']
        assert 'median_seconds' not in timed
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
