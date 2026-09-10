#!/usr/bin/env python3

import argparse
import hashlib
import html
import json
import math
import pathlib
import sys


EARTH_RADIUS_M=6371000.0
TIME_UNITS="seconds since 1968-05-23 00:00:00 GMT"


def checksum(path):
    digest=hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda:stream.read(1024*1024),b""):
            digest.update(block)
    return digest.hexdigest()


def quantile(values,fraction):
    ordered=sorted(values)
    if not ordered:
        return None
    position=fraction*(len(ordered)-1)
    lower=int(math.floor(position)); upper=int(math.ceil(position))
    return ordered[lower]+(ordered[upper]-ordered[lower])*(position-lower)


def circular_mean_degrees(values):
    x=sum(math.cos(math.radians(value)) for value in values)
    y=sum(math.sin(math.radians(value)) for value in values)
    return math.degrees(math.atan2(y,x))


def longitude_near(value,reference):
    while value-reference>180: value-=360
    while value-reference<-180: value+=360
    return value


def haversine(latitude0,longitude0,latitude1,longitude1):
    phi0=math.radians(latitude0); phi1=math.radians(latitude1)
    dphi=phi1-phi0; dlambda=math.radians(longitude_near(longitude1,longitude0)-longitude0)
    a=math.sin(dphi/2)**2+math.cos(phi0)*math.cos(phi1)*math.sin(dlambda/2)**2
    return 2*EARTH_RADIUS_M*math.atan2(math.sqrt(a),math.sqrt(max(0,1-a)))


def read_snapshots(paths,include_inactive=False):
    try:
        import netCDF4
    except ImportError as error:
        raise RuntimeError("netCDF4 is required to read WaComM++ trajectory output") from error
    snapshots=[]
    for path in paths:
        with netCDF4.Dataset(path) as dataset:
            missing=[name for name in ("id","lat","lon","particle_time") if name not in dataset.variables]
            if missing:
                raise ValueError(f"{path}: missing required variables: {', '.join(missing)}")
            identifiers=dataset.variables["id"][:]
            latitude=dataset.variables["lat"][:]
            longitude=dataset.variables["lon"][:]
            times=dataset.variables["particle_time"][:]
            time_units=getattr(dataset.variables["particle_time"],"units","")
            if time_units!=TIME_UNITS:
                raise ValueError(f"{path}: particle_time units must be '{TIME_UNITS}'")
            if len(times)!=1 or latitude.ndim!=2 or longitude.shape!=latitude.shape or latitude.shape[1]!=1:
                raise ValueError(f"{path}: expected one [particles, particle_time=1] snapshot")
            if latitude.shape[0]!=len(identifiers):
                raise ValueError(f"{path}: particle coordinate and identity dimensions differ")
            health=dataset.variables["health"][:] if "health" in dataset.variables else None
            points=[]
            for index,identifier in enumerate(identifiers):
                lat=float(latitude[index,0]); lon=float(longitude[index,0])
                active=health is None or float(health[index,0])>0
                if math.isfinite(lat) and math.isfinite(lon) and (include_inactive or active):
                    points.append({"id":int(identifier),"latitude":lat,"longitude":lon})
            provenance={name:str(getattr(dataset,name)) for name in
                        ("wacomm_git_revision","wacomm_compiler","wacomm_cmake_options","wacomm_configuration")
                        if name in dataset.ncattrs()}
            snapshots.append({"path":str(path.resolve()),"sha256":checksum(path),"provenance":provenance,
                              "time":float(times[0]),"points":points})
    snapshots.sort(key=lambda item:item["time"])
    if len({item["time"] for item in snapshots})!=len(snapshots):
        raise ValueError("input files contain duplicate particle_time values")
    if not any(item["points"] for item in snapshots):
        raise ValueError("no finite selected particle positions were found")
    return snapshots


def summarize(snapshots):
    summaries=[]
    for snapshot in snapshots:
        points=snapshot["points"]
        if not points:
            summaries.append({"time":snapshot["time"],"members":0})
            continue
        latitudes=[point["latitude"] for point in points]
        longitudes=[point["longitude"] for point in points]
        centroid_latitude=sum(latitudes)/len(latitudes)
        centroid_longitude=circular_mean_degrees(longitudes)
        unwrapped=[longitude_near(value,centroid_longitude) for value in longitudes]
        distances=[haversine(centroid_latitude,centroid_longitude,point["latitude"],point["longitude"])
                   for point in points]
        summaries.append({"time":snapshot["time"],"members":len(points),
                          "centroid":{"latitude_degrees_north":centroid_latitude,
                                      "longitude_degrees_east":centroid_longitude},
                          "extent":{"west_degrees_east":min(unwrapped),"east_degrees_east":max(unwrapped),
                                    "south_degrees_north":min(latitudes),"north_degrees_north":max(latitudes)},
                          "radial_distance_from_centroid_m":{"median":quantile(distances,.5),
                                                             "p90":quantile(distances,.9),
                                                             "maximum":max(distances)}})
    return summaries


def render_svg(snapshots,path,width=1000,height=650):
    points=[point for snapshot in snapshots for point in snapshot["points"]]
    center=circular_mean_degrees([point["longitude"] for point in points])
    longitudes=[longitude_near(point["longitude"],center) for point in points]
    latitudes=[point["latitude"] for point in points]
    west,east=min(longitudes),max(longitudes); south,north=min(latitudes),max(latitudes)
    lon_pad=max((east-west)*.05,.01); lat_pad=max((north-south)*.05,.01)
    west-=lon_pad; east+=lon_pad; south-=lat_pad; north+=lat_pad
    margin=75
    def xy(latitude,longitude):
        x=margin+(longitude_near(longitude,center)-west)/(east-west)*(width-2*margin)
        y=height-margin-(latitude-south)/(north-south)*(height-2*margin)
        return x,y
    trajectories={}
    for snapshot in snapshots:
        for point in snapshot["points"]:
            trajectories.setdefault(point["id"],[]).append((snapshot["time"],*xy(point["latitude"],point["longitude"])))
    time0=snapshots[0]["time"]; time1=snapshots[-1]["time"]
    description=(f"Diagnostic Plate Carree trajectory map in EPSG:4326 spanning longitude {west:.6f} to {east:.6f} "
                 f"degrees east and latitude {south:.6f} to {north:.6f} degrees north; time {time0:.3f} to {time1:.3f} "
                 "seconds since 1968-05-23 00:00:00 GMT. No coastline or basemap is used. Lines are ensemble members, not probabilities.")
    lines=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" role="img" aria-labelledby="title description">',
           '<title id="title">WaComM++ trajectory ensemble diagnostic</title>',
           f'<desc id="description">{html.escape(description)}</desc>',
           f'<rect width="{width}" height="{height}" fill="#f8fafc"/>',
           f'<rect x="{margin}" y="{margin}" width="{width-2*margin}" height="{height-2*margin}" fill="#e0f2fe" stroke="#334155" stroke-width="2"/>']
    palette=("#1d4ed8","#15803d","#b91c1c","#7e22ce","#c2410c","#0f766e")
    for index,(identifier,values) in enumerate(sorted(trajectories.items())):
        values.sort()
        coordinates=" ".join(f"{x:.2f},{y:.2f}" for _,x,y in values)
        color=palette[index%len(palette)]
        if len(values)>1:
            lines.append(f'<polyline points="{coordinates}" fill="none" stroke="{color}" stroke-width="2" opacity="0.72"/>')
        for _,x,y in values:
            lines.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="3" fill="{color}"/>')
    lines.extend([f'<g font-family="sans-serif" fill="#172033"><text x="{width/2}" y="36" text-anchor="middle" font-size="22">WaComM++ trajectory ensemble diagnostic</text>',
                  f'<text x="{margin}" y="{height-38}" font-size="14">EPSG:4326 Plate Carrée | lon [{west:.4f}, {east:.4f}]°E | lat [{south:.4f}, {north:.4f}]°N</text>',
                  f'<text x="{margin}" y="{height-18}" font-size="14">time [{time0:.1f}, {time1:.1f}] s since 1968-05-23 UTC | no coastline | lines are members, not probability contours</text></g>',
                  '</svg>'])
    path.write_text("\n".join(lines)+"\n",encoding="utf-8")
    return {"projection":"EPSG:4326 Plate Carree","extent":{"west":west,"east":east,"south":south,"north":north},
            "coastline":"none","uncertainty_semantics":"Each line is a selected trajectory member; no probability density or confidence region is inferred."}


def main():
    parser=argparse.ArgumentParser(description="Create reproducible diagnostics and an SVG map from WaComM++ particle snapshots")
    parser.add_argument("inputs",nargs="+",type=pathlib.Path)
    parser.add_argument("--json",required=True,type=pathlib.Path,dest="json_output")
    parser.add_argument("--svg",required=True,type=pathlib.Path)
    parser.add_argument("--include-inactive",action="store_true")
    arguments=parser.parse_args()
    try:
        snapshots=read_snapshots(arguments.inputs,arguments.include_inactive)
        arguments.json_output.parent.mkdir(parents=True,exist_ok=True); arguments.svg.parent.mkdir(parents=True,exist_ok=True)
        map_metadata=render_svg(snapshots,arguments.svg)
        result={"schema":"wacomm-trajectory-diagnostics-v1","inputs":[{"path":item["path"],"sha256":item["sha256"],"provenance":item["provenance"]} for item in snapshots],
                "selection":{"include_inactive":arguments.include_inactive,"finite_coordinates_required":True},
                "time_units":TIME_UNITS,"diagnostics":summarize(snapshots),"map":map_metadata}
        arguments.json_output.write_text(json.dumps(result,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    except (OSError,RuntimeError,ValueError) as error:
        print(f"trajectory_diagnostics: {error}",file=sys.stderr)
        return 2
    return 0


if __name__=="__main__":
    raise SystemExit(main())
