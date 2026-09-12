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
import scaling_figures as scaling

config={"io":{"ocean_model":"WaComM","base_path":"processed-6h/","save_input":False,
              "nc_inputs":[f"ocm3_d03_20210701Z{hour}.nc" for hour in ("09","10","11","13","14","15")]}}
scaling.validate_forcing(config)
for key,value in (("ocean_model","ROMS"),("base_path","roms/"),("save_input",True),("nc_inputs",[])):
    invalid={"io":dict(config["io"],**{key:value})}
    try:scaling.validate_forcing(invalid);raise AssertionError("invalid forcing accepted")
    except ValueError:pass

records=[{"processes":p,"threads_per_process":1,"elapsed_seconds":128/p,"solver_seconds":64/p} for p in scaling.PROCESSES]
result=scaling.metrics(list(reversed(records)))
assert [r["speedup"] for r in result]==scaling.PROCESSES
assert all(r["efficiency"]==1 for r in result)
extended=records+[dict(records[0],processes=64,elapsed_seconds=2,solver_seconds=1)]
assert scaling.metrics(extended)[-1]["speedup"]==64
records[-1]["solver_seconds"]=4
assert scaling.metrics(records)[-1]["speedup"]==16
assert scaling.metrics(records)[-1]["efficiency"]==0.5
for key in ("elapsed_seconds","solver_seconds"):
 for bad in (0,-1,float("nan"),float("inf")):
    invalid=[dict(r) for r in records];invalid[0][key]=bad
    try:scaling.metrics(invalid);raise AssertionError("invalid time accepted")
    except ValueError:pass
for invalid in (records[:-1],records+[records[0]], [dict(r,threads_per_process=2) for r in records]):
    try:scaling.metrics(invalid);raise AssertionError("invalid experiment accepted")
    except ValueError:pass
times=[1675933200,1675936800,1675940400,1675947600,1675951200,1675954800]
lines=[f"Solver interval: start={a} end={b} seconds=0.25" for a,b in zip(times[:-1],times[1:])]
assert sum(r["seconds"] for r in scaling.solver_intervals("\n".join(lines)))==1.25
for invalid in (lines[:-1],lines+lines[:1],list(reversed(lines)),[line.replace("0.25","nan") for line in lines]):
    try:scaling.solver_intervals("\n".join(invalid));raise AssertionError("invalid interval records accepted")
    except ValueError:pass
with tempfile.TemporaryDirectory() as directory:
    artifacts=scaling.render(scaling.metrics(records),pathlib.Path(directory))
    assert len(artifacts)==6
    for artifact in artifacts:
        path=pathlib.Path(directory)/artifact["name"]
        assert path.stat().st_size>1000
        if path.suffix==".svg":
            root=ET.parse(path).getroot()
            assert root.find("{http://www.w3.org/2000/svg}title") is not None
            assert root.find("{http://www.w3.org/2000/svg}desc") is not None
