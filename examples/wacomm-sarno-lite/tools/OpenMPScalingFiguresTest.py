#!/usr/bin/env python3

import pathlib
import sys
import tempfile
import xml.etree.ElementTree as ET

try:
    import matplotlib
    import netCDF4
    import numpy
except ImportError:
    raise SystemExit(77)
sys.path.insert(0,str(pathlib.Path(sys.argv[1]).resolve().parent))
import openmp_scaling_figures as omp

records=[{"processes":1,"threads_per_process":n,"solver_seconds":64/n,"elapsed_seconds":128/n} for n in omp.THREADS]
result=omp.metrics(list(reversed(records)))
assert [r["speedup"] for r in result]==omp.THREADS
assert all(r["efficiency"]==1 for r in result)
records[-1]["solver_seconds"]=4
assert omp.metrics(records)[-1]["speedup"]==16
assert omp.metrics(records)[-1]["efficiency"]==.5
for key in ("solver_seconds","elapsed_seconds"):
    for bad in (0,-1,float("nan"),float("inf")):
        invalid=[dict(r) for r in records];invalid[0][key]=bad
        try:omp.metrics(invalid);raise AssertionError("invalid timing accepted")
        except ValueError:pass
for invalid in (records[:-1],records+[records[0]],[dict(r,processes=2) for r in records]):
    try:omp.metrics(invalid);raise AssertionError("invalid experiment accepted")
    except ValueError:pass
assert omp.affinity("OMP_PLACES = '{0}'\nrank 0 bound to socket 0[core 0[hwt 0]]",1)==[0]
assert omp.affinity("level 1 thread A affinity 0\nlevel 1 thread B affinity 1\n",2)==[0,1]
for invalid in ("level 1 thread A affinity 0\n", "level 1 thread A affinity 0\nlevel 1 thread B affinity 0\n"):
    try:omp.affinity(invalid,2);raise AssertionError("missing or shared core accepted")
    except ValueError:pass
with tempfile.TemporaryDirectory() as directory:
    mpi=[{"processes":n,"solver_seconds":80/n,"speedup":n,"efficiency":1} for n in omp.THREADS]
    artifacts=omp.render(omp.metrics(records),mpi,pathlib.Path(directory))
    assert len(artifacts)==6
    for artifact in artifacts:
        path=pathlib.Path(directory)/artifact["name"]
        assert path.stat().st_size>1000
        if path.suffix==".svg":
            root=ET.parse(path).getroot()
            assert root.find("{http://www.w3.org/2000/svg}title") is not None
            assert root.find("{http://www.w3.org/2000/svg}desc") is not None
            assert "32" in path.read_text()
