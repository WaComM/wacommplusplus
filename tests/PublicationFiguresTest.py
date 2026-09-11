#!/usr/bin/env python3

import json
import pathlib
import subprocess
import sys
import tempfile

try:
    import netCDF4
    import numpy as np
    import matplotlib
except ImportError:
    raise SystemExit(77)

script=pathlib.Path(sys.argv[1]).resolve()
sys.path.insert(0,str(script.parent))
import publication_figures as figures


def snapshot(path,time,ids,depth,health):
    with netCDF4.Dataset(path,"w") as d:
        d.createDimension("particles",len(ids));d.createDimension("particle_time",1)
        d.createVariable("id","u8",("particles",))[:]=ids
        v=d.createVariable("particle_time","f8",("particle_time",));v.units=figures.TIME_UNITS;v[:]=[time]
        for name,data,units in (("lon",[14.4]*len(ids),"degree_east"),("lat",[40.7]*len(ids),"degree_north"),
                                ("depth",depth,"meters"),("health",health,"1"),("age",[3600]*len(ids),"seconds since emission")):
            v=d.createVariable(name,"f8",("particles","particle_time"),fill_value=-999)
            v.unit=units;v[:]=np.array(data).reshape(-1,1)
            if name=="depth":v.positive="up"


with tempfile.TemporaryDirectory() as directory:
    root=pathlib.Path(directory);a=root/"a.nc";b=root/"b.nc";grid=root/"grid.nc"
    snapshot(a,3600,[2**53+1,2**53+2,3,4],[-1,-2,-3,-999],[1,1,0,1])
    snapshot(b,7200,[2**53+1,2**53+2],[-2,-4],[1,1])
    before={p:figures.checksum(p) for p in (a,b)}
    records=figures.read_particles([b,a])
    assert records[0]["points"]["id"].tolist()==[2**53+1,2**53+2]
    assert records[0]["input"]["excluded_invalid"]==1
    assert records[0]["input"]["excluded_inactive"]==1
    summary=figures.statistics(records,[14.4,40.7],np.array([0,1,2,3,4]))
    assert summary[0]["depth_quantiles_m"]==[1.1,1.5,1.9]
    assert summary[0]["depth_counts"]==[0,1,1,0]
    assert summary[1]["depth_counts"]==[0,0,1,1]
    assert summary[0]["radial_distance_from_source_m_quantiles"]==[0,0,0]
    try:figures.read_particles([a,a]);raise AssertionError("duplicate time accepted")
    except ValueError as e:assert "duplicate physical" in str(e)
    with netCDF4.Dataset(grid,"w") as d:
        d.createDimension("j",4);d.createDimension("i",4)
        lon,lat=np.meshgrid(np.linspace(14.3,14.5,4),np.linspace(40.6,40.8,4))
        for name,data,units in (("lon_rho",lon,"degree_east"),("lat_rho",lat,"degree_north"),
                                ("mask_rho",(lon<14.46).astype(float),"1"),("h",(14.5-lon)*400,"meter")):
            v=d.createVariable(name,"f8",("j","i"));v.units=units;v[:]=data
    args=["--grid",str(grid),"--map-crs","EPSG:4326","--source","14.4","40.7",
          "--extent","14.3","14.5","40.6","40.8","--depth-bin","1"]
    for name,inputs in (("first",[b,a]),("second",[a,b])):
        subprocess.run([sys.executable,str(script),*[str(p) for p in inputs],*args,"--output-dir",str(root/name)],check=True)
    first=json.loads((root/"first/sarno-figure-manifest.json").read_text())
    second=json.loads((root/"second/sarno-figure-manifest.json").read_text())
    assert first==second, "input ordering must not alter figures or diagnostics"
    for name in ("sarno-maps","sarno-profiles"):
        svg=(root/"first"/(name+".svg")).read_text()
        assert "<title>" in svg and "<desc>" in svg and "EPSG:4326" in svg
    assert all(figures.checksum(p)==h for p,h in before.items()), "diagnostics modified inputs"
    with netCDF4.Dataset(b,"a") as d:d.variables["depth"].unit="fathoms"
    try:figures.read_particles([b]);raise AssertionError("unknown units accepted")
    except ValueError as e:assert "metre" in str(e)
    with netCDF4.Dataset(b,"a") as d:
        d.variables["depth"].unit="meters";d.variables["id"][:]=[1,1]
    try:figures.read_particles([b]);raise AssertionError("duplicate ids accepted")
    except ValueError as e:assert "duplicate trajectory" in str(e)

    with netCDF4.Dataset(grid,"a") as d:d.variables["mask_rho"][0,0]=2
    try:figures.read_grid(grid,[14.3,14.5,40.6,40.8]);raise AssertionError("nonbinary mask accepted")
    except ValueError as e:assert "binary" in str(e)
    with netCDF4.Dataset(grid,"a") as d:
        d.variables["mask_rho"][0,0]=1;d.variables["lon_rho"][:]=14.4
    try:figures.read_grid(grid,[14.3,14.5,40.6,40.8]);raise AssertionError("degenerate grid accepted")
    except ValueError as e:assert "degenerate" in str(e)
