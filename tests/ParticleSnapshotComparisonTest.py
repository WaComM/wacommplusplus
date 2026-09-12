#!/usr/bin/env python3

import pathlib
import sys
import tempfile

try:
    import netCDF4
    import numpy as np
except ImportError:
    raise SystemExit(77)
sys.path.insert(0,str(pathlib.Path(sys.argv[1]).resolve().parent))
import compare_particle_snapshots as comparison


def write(path,reverse=False):
    with netCDF4.Dataset(path,"w") as d:
        d.createDimension("particles",2);d.createDimension("particle_time",1)
        v=d.createVariable("particle_time","f8",("particle_time",));v[:]=[3600]
        v.units=comparison.TIME_UNITS;v.calendar="gregorian"
        ids=np.array([2**53+1,2**53+2],dtype="u8")
        depth=np.array([[-1.],[-2.]])
        if reverse:ids=ids[::-1];depth=depth[::-1]
        d.createVariable("id","u8",("particles",))[:]=ids
        v=d.createVariable("depth","f8",("particles","particle_time"));v[:]=depth;v.unit="meters";v.positive="up"
        d.wacomm_compiler="different build" if reverse else "reference build"


with tempfile.TemporaryDirectory() as directory:
    root=pathlib.Path(directory);a=root/"reference";b=root/"candidate";a.mkdir();b.mkdir()
    write(a/"a.nc");write(b/"b.nc",True)
    hashes=[comparison.checksum(p) for p in (a/"a.nc",b/"b.nc")]
    result=comparison.compare(a,b)
    assert result["equal"] and result["snapshots"][0]["particles"]==2
    assert hashes==[comparison.checksum(p) for p in (a/"a.nc",b/"b.nc")]
    with netCDF4.Dataset(b/"b.nc","a") as d:d.variables["depth"][0,0]=-2.00001
    try:comparison.compare(a,b);raise AssertionError("different depth accepted")
    except ValueError as error:assert "values differ" in str(error)
    write(b/"b.nc",True)
    with netCDF4.Dataset(b/"b.nc","a") as d:d.variables["id"][:]=[1,1]
    try:comparison.compare(a,b);raise AssertionError("duplicate IDs accepted")
    except ValueError as error:assert "duplicate" in str(error)
    write(b/"b.nc",True)
    with netCDF4.Dataset(b/"b.nc","a") as d:d.variables["particle_time"][:]=[7200]
    try:comparison.compare(a,b);raise AssertionError("different times accepted")
    except ValueError as error:assert "times differ" in str(error)
