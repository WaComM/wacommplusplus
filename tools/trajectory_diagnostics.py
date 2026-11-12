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


def render_html(snapshots,path,map_metadata):
    payload={"time_units":TIME_UNITS,"snapshots":[{"time":item["time"],"points":item["points"]} for item in snapshots],
             "map":map_metadata}
    encoded=json.dumps(payload,sort_keys=True,separators=(",",":")).replace("<","\\u003c")
    document="""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>WaComM++ interactive trajectory diagnostic</title>
<style>
:root{color-scheme:light dark;--ink:#172033;--water:#e0f2fe;--panel:#f8fafc;--grid:#64748b} @media(prefers-color-scheme:dark){:root{--ink:#e2e8f0;--water:#123047;--panel:#0f172a;--grid:#94a3b8}}
body{font:15px system-ui,sans-serif;color:var(--ink);background:var(--panel);max-width:1100px;margin:auto;padding:1rem} h1{font-size:1.45rem} .controls{display:flex;gap:1rem;align-items:center;flex-wrap:wrap} input[type=range]{flex:1;min-width:260px} svg{width:100%;height:auto;background:var(--water);border:1px solid var(--grid)} .trajectory{fill:none;stroke-width:2;opacity:.7}.trajectory.dimmed{opacity:.08}.point{stroke:var(--panel);stroke-width:1}.point.dimmed{opacity:.08} table{border-collapse:collapse;margin-top:.75rem}th,td{padding:.3rem .7rem;border:1px solid var(--grid);text-align:right}th:first-child{text-align:left}.note{font-size:.9rem}.focus{stroke-width:5;opacity:1}
</style></head><body><h1>WaComM++ interactive trajectory diagnostic</h1>
<p class="note">EPSG:4326 Plate Carrée; no coastline or basemap. Lines are selected trajectory members, not probabilities, confidence regions, or search areas.</p>
<div class="controls"><label for="time">Physical time</label><input id="time" type="range" min="0" step="1"><output id="time-label"></output><button id="play" type="button">Play</button><label><input id="paths" type="checkbox" checked> Show complete paths</label></div>
<svg id="map" viewBox="0 0 1000 620" role="img" aria-labelledby="map-title map-description"><title id="map-title">Interactive trajectory ensemble map</title><desc id="map-description">Use the time slider or arrow keys to inspect stable trajectory members. Select a point or table row to highlight its member.</desc><g id="grid"></g><g id="tracks"></g><g id="points"></g></svg>
<table><thead><tr><th>Selected snapshot</th><th>Members</th><th>Centroid latitude (°N)</th><th>Centroid longitude (°E)</th></tr></thead><tbody><tr><th id="selected-time"></th><td id="member-count"></td><td id="centroid-lat"></td><td id="centroid-lon"></td></tr></tbody></table>
<p id="selection" aria-live="polite">No member selected.</p><script id="trajectory-data" type="application/json">__DATA__</script>
<script>
const data=JSON.parse(document.getElementById('trajectory-data').textContent),svg=document.getElementById('map'),NS='http://www.w3.org/2000/svg',margin=65;
const ext=data.map.extent,lon=v=>margin+(v-ext.west)/(ext.east-ext.west)*(1000-2*margin),lat=v=>620-margin-(v-ext.south)/(ext.north-ext.south)*(620-2*margin);
const near=v=>{while(v-ext.west>360)v-=360;while(v<ext.west)v+=360;return v},el=(name,attrs,parent)=>{const n=document.createElementNS(NS,name);for(const[k,v]of Object.entries(attrs))n.setAttribute(k,v);parent.appendChild(n);return n};
const grid=document.getElementById('grid');el('rect',{x:margin,y:margin,width:870,height:490,fill:'none',stroke:'var(--grid)'},grid);
for(let i=0;i<=4;i++){const x=margin+i*870/4,y=margin+i*490/4;el('line',{x1:x,y1:margin,x2:x,y2:555,stroke:'var(--grid)','stroke-opacity':.25},grid);el('line',{x1:margin,y1:y,x2:935,y2:y,stroke:'var(--grid)','stroke-opacity':.25},grid)}
const members=new Map;data.snapshots.forEach((s,si)=>s.points.forEach(p=>{if(!members.has(p.id))members.set(p.id,[]);members.get(p.id).push({...p,time:s.time,si})}));
const palette=['#2563eb','#16a34a','#dc2626','#9333ea','#ea580c','#0d9488'],tracks=document.getElementById('tracks'),points=document.getElementById('points');let selected=null,timer=null;
for(const[id,values]of [...members].sort((a,b)=>a[0]-b[0])){const color=palette[Number(id)%palette.length],line=el('polyline',{points:values.map(p=>`${lon(near(p.longitude))},${lat(p.latitude)}`).join(' '),stroke:color,class:'trajectory','data-id':id},tracks);line.addEventListener('click',()=>select(id));values.forEach(p=>{const c=el('circle',{cx:lon(near(p.longitude)),cy:lat(p.latitude),r:5,fill:color,class:'point','data-id':id,'data-snapshot':p.si,tabindex:0,role:'button','aria-label':`trajectory ${id}, time ${p.time}, latitude ${p.latitude}, longitude ${p.longitude}`},points);c.addEventListener('click',()=>select(id));c.addEventListener('keydown',e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();select(id)}})})}
function circular(values){let x=0,y=0;values.forEach(v=>{x+=Math.cos(v*Math.PI/180);y+=Math.sin(v*Math.PI/180)});return Math.atan2(y,x)*180/Math.PI}
function select(id){selected=id;document.querySelectorAll('[data-id]').forEach(n=>{n.classList.toggle('dimmed',String(n.dataset.id)!==String(id));n.classList.toggle('focus',String(n.dataset.id)===String(id))});document.getElementById('selection').textContent=`Selected trajectory ${id}.`}
function update(){const i=+slider.value,s=data.snapshots[i],lats=s.points.map(p=>p.latitude),lons=s.points.map(p=>p.longitude);document.querySelectorAll('.point').forEach(n=>n.hidden=+n.dataset.snapshot!==i);document.getElementById('time-label').value=`${s.time} s since 1968-05-23 UTC`;document.getElementById('selected-time').textContent=s.time;document.getElementById('member-count').textContent=s.points.length;document.getElementById('centroid-lat').textContent=lats.length?(lats.reduce((a,b)=>a+b,0)/lats.length).toFixed(6):'—';document.getElementById('centroid-lon').textContent=lons.length?circular(lons).toFixed(6):'—'}
const slider=document.getElementById('time');slider.max=data.snapshots.length-1;slider.addEventListener('input',update);slider.addEventListener('keydown',e=>{if(e.key==='Home'){slider.value=0;update()}if(e.key==='End'){slider.value=slider.max;update()}});document.getElementById('paths').addEventListener('change',e=>tracks.hidden=!e.target.checked);document.getElementById('play').addEventListener('click',e=>{if(timer){clearInterval(timer);timer=null;e.target.textContent='Play'}else{e.target.textContent='Pause';timer=setInterval(()=>{slider.value=(+slider.value+1)%data.snapshots.length;update()},700)}});update();
</script></body></html>
""".replace("__DATA__",encoded)
    path.write_text(document,encoding="utf-8")


def main():
    parser=argparse.ArgumentParser(description="Create reproducible diagnostics and static or interactive maps from WaComM++ particle snapshots")
    parser.add_argument("inputs",nargs="+",type=pathlib.Path)
    parser.add_argument("--json",required=True,type=pathlib.Path,dest="json_output")
    parser.add_argument("--svg",required=True,type=pathlib.Path)
    parser.add_argument("--html",type=pathlib.Path,help="write a self-contained interactive HTML diagnostic")
    parser.add_argument("--include-inactive",action="store_true")
    arguments=parser.parse_args()
    try:
        snapshots=read_snapshots(arguments.inputs,arguments.include_inactive)
        arguments.json_output.parent.mkdir(parents=True,exist_ok=True); arguments.svg.parent.mkdir(parents=True,exist_ok=True)
        map_metadata=render_svg(snapshots,arguments.svg)
        if arguments.html:
            arguments.html.parent.mkdir(parents=True,exist_ok=True)
            render_html(snapshots,arguments.html,map_metadata)
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
