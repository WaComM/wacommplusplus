import json
import math
import pathlib
import subprocess
import sys
import tempfile

try:
    import netCDF4
    import numpy
except ImportError:
    raise SystemExit(77)


def snapshot(path,time,identifiers,latitude,longitude,health=None):
    with netCDF4.Dataset(path,"w",format="NETCDF4") as dataset:
        particles=dataset.createDimension("particles",len(identifiers))
        particle_time=dataset.createDimension("particle_time",1)
        dataset.createVariable("id","u8",("particles",))[:]=numpy.asarray(identifiers,dtype="u8")
        time_variable=dataset.createVariable("particle_time","f8",("particle_time",))
        time_variable.units="seconds since 1968-05-23 00:00:00 GMT"
        time_variable[:]=[time]
        dataset.wacomm_git_revision="test-revision"
        dataset.createVariable("lat","f8",("particles","particle_time"))[:]=numpy.asarray(latitude).reshape(-1,1)
        dataset.createVariable("lon","f8",("particles","particle_time"))[:]=numpy.asarray(longitude).reshape(-1,1)
        if health is not None:
            dataset.createVariable("health","f8",("particles","particle_time"))[:]=numpy.asarray(health).reshape(-1,1)


with tempfile.TemporaryDirectory() as directory:
    root=pathlib.Path(directory)
    later=root/"later.nc"; earlier=root/"earlier.nc"
    snapshot(earlier,0,[7,8],[40,40],[179.8,-179.8],[1,1])
    snapshot(later,60,[7,8,9],[40.1,40.2,0],[179.9,-179.7,0],[1,1,-1])
    output_json=root/"diagnostics.json"; output_svg=root/"map.svg"
    subprocess.run([sys.executable,sys.argv[1],str(later),str(earlier),"--json",str(output_json),"--svg",str(output_svg)],check=True)
    result=json.loads(output_json.read_text())
    assert result["schema"]=="wacomm-trajectory-diagnostics-v1"
    assert [item["time"] for item in result["diagnostics"]]==[0,60]
    assert [item["members"] for item in result["diagnostics"]]==[2,2]
    assert result["diagnostics"][0]["extent"]["east_degrees_east"]-result["diagnostics"][0]["extent"]["west_degrees_east"]<1
    assert result["diagnostics"][1]["radial_distance_from_centroid_m"]["maximum"]>0
    assert result["map"]["coastline"]=="none"
    assert result["inputs"][0]["provenance"]["wacomm_git_revision"]=="test-revision"
    svg=output_svg.read_text()
    assert "EPSG:4326" in svg and "not probability contours" in svg

    included=root/"included.json"
    subprocess.run([sys.executable,sys.argv[1],str(later),"--json",str(included),"--svg",str(root/"included.svg"),
                    "--include-inactive"],check=True)
    assert json.loads(included.read_text())["diagnostics"][0]["members"]==3

    duplicate=subprocess.run([sys.executable,sys.argv[1],str(earlier),str(earlier),"--json",str(root/"bad.json"),
                              "--svg",str(root/"bad.svg")],capture_output=True,text=True)
    assert duplicate.returncode==2 and "duplicate particle_time" in duplicate.stderr
