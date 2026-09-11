#!/usr/bin/env python3

import argparse
import datetime
import json
import pathlib
import platform

import numpy as np
import netCDF4
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import Normalize

from trajectory_diagnostics import checksum, haversine, TIME_UNITS


def values(variable):
    return np.ma.asarray(variable[:], dtype=float).filled(np.nan)


def unit(variable):
    return getattr(variable, "units", getattr(variable, "unit", ""))


def read_particles(paths):
    snapshots=[]
    for path in paths:
        with netCDF4.Dataset(path) as data:
            required=("id", "particle_time", "lon", "lat", "depth", "health", "age")
            if any(name not in data.variables for name in required):
                raise ValueError(f"{path}: required particle variables are {required}")
            v=data.variables
            if unit(v["particle_time"])!=TIME_UNITS or getattr(v["particle_time"], "calendar", "gregorian")!="gregorian":
                raise ValueError("particle_time requires the WaComM Gregorian epoch")
            time=values(v["particle_time"])
            if time.shape!=(1,) or not np.isfinite(time[0]):
                raise ValueError("expected one finite physical time per snapshot")
            ids=np.ma.asarray(v["id"][:])
            if ids.ndim!=1 or ids.dtype.kind not in "ui" or np.ma.getmaskarray(ids).any():
                raise ValueError("unmasked integer trajectory identifiers are required")
            ids=np.asarray(ids)
            if np.unique(ids).size!=ids.size:
                raise ValueError("duplicate trajectory identifiers")
            if unit(v["lon"]) not in ("degree_east", "degrees_east") or unit(v["lat"]) not in ("degree_north", "degrees_north"):
                raise ValueError("geographic angular coordinate units are required")
            if unit(v["depth"]) not in ("m", "meter", "meters") or getattr(v["depth"], "positive", "") not in ("up", "down"):
                raise ValueError("depth requires metre units and explicit positive direction")
            if unit(v["age"]) not in ("s", "second", "seconds", "seconds since emission"):
                raise ValueError("age requires seconds")
            arrays={name:values(v[name]) for name in ("lon", "lat", "depth", "health", "age")}
            if any(a.shape!=(len(ids),1) for a in arrays.values()):
                raise ValueError("expected [particles, particle_time=1] arrays")
            arrays={name:a[:,0] for name,a in arrays.items()}
            valid=np.logical_and.reduce([np.isfinite(a) & (np.abs(a)<1e30) for a in arrays.values()])
            valid &= (np.abs(arrays["lat"])<=90) & (np.abs(arrays["lon"])<=180)
            valid &= arrays["age"]>=0
            active=valid & (arrays["health"]>0)
            points={name:a[active] for name,a in arrays.items()}
            if getattr(v["depth"], "positive")=="up": points["depth"]=-points["depth"]
            points["id"]=ids[active]
            snapshots.append({"time":float(time[0]), "points":points,
                              "input":{"name":path.name,"sha256":checksum(path),
                                       "excluded_invalid":int((~valid).sum()),
                                       "excluded_inactive":int((valid & ~active).sum()),
                                       "provenance":{key:str(data.getncattr(key)) for key in data.ncattrs() if key.startswith("wacomm_")}}})
    snapshots.sort(key=lambda s:s["time"])
    if len({s["time"] for s in snapshots})!=len(snapshots):
        raise ValueError("duplicate physical snapshot times")
    if not snapshots or any(len(s["points"]["id"])==0 for s in snapshots):
        raise ValueError("every snapshot must contain selected particles")
    return snapshots


def utc(seconds):
    return datetime.datetime(1968,5,23,tzinfo=datetime.timezone.utc)+datetime.timedelta(seconds=seconds)


def statistics(snapshots,source,bins):
    result=[]
    for s in snapshots:
        p=s["points"]; depth=p["depth"]
        count,_=np.histogram(depth,bins)
        result.append({"utc":utc(s["time"]).isoformat(),"time":s["time"],"selected":len(depth),
                       "depth_quantiles_m":np.quantile(depth,[.1,.5,.9],method="linear").tolist(),
                       "depth_counts":count.tolist(),"depth_outside_bins":int(len(depth)-count.sum()),
                       "radial_distance_from_source_m_quantiles":np.quantile(
                           [haversine(source[1],source[0],lat,lon) for lat,lon in zip(p["lat"],p["lon"])],
                           [.1,.5,.9],method="linear").tolist()})
    return result


def read_grid(path,extent):
    with netCDF4.Dataset(path) as data:
        v=data.variables
        if any(name not in v for name in ("lon_rho","lat_rho","mask_rho","h")):
            raise ValueError("grid requires lon_rho, lat_rho, mask_rho, h")
        if unit(v["lon_rho"]) not in ("degree_east","degrees_east") or unit(v["lat_rho"]) not in ("degree_north","degrees_north") or unit(v["h"]) not in ("meter","meters","m"):
            raise ValueError("grid coordinate or bathymetry units are unsupported")
        lon=values(v["lon_rho"]);lat=values(v["lat_rho"])
        mask=values(v["mask_rho"]);depth=values(v["h"])
        if lon.ndim!=2 or any(a.shape!=lon.shape for a in (lat,mask,depth)):
            raise ValueError("grid arrays must have matching two-dimensional shapes")
        if not np.isfinite(lon).all() or not np.isfinite(lat).all() or not np.isin(mask,[0,1]).all():
            raise ValueError("grid coordinates must be finite and mask binary")
        if not np.isfinite(depth[mask==1]).all() or (depth[mask==1]<0).any():
            raise ValueError("wet bathymetry must be finite and nonnegative")
        inside=(lon>=extent[0])&(lon<=extent[1])&(lat>=extent[2])&(lat<=extent[3])
        rows,cols=np.where(inside)
        if not len(rows): raise ValueError("map extent does not intersect the supplied grid")
        sl=(slice(max(0,rows.min()-2),min(lon.shape[0],rows.max()+3)),
            slice(max(0,cols.min()-2),min(lon.shape[1],cols.max()+3)))
        cropped=[a[sl] for a in (lon,lat,mask,depth)]
        x,y=cropped[:2]
        if min(x.shape)<2: raise ValueError("grid crop requires at least two rows and columns")
        corners=[(x[:-1,:-1],y[:-1,:-1]),(x[:-1,1:],y[:-1,1:]),
                 (x[1:,1:],y[1:,1:]),(x[1:,:-1],y[1:,:-1])]
        cross=[]
        for i in range(4):
            a,b,c=corners[i],corners[(i+1)%4],corners[(i+2)%4]
            cross.append((b[0]-a[0])*(c[1]-b[1])-(b[1]-a[1])*(c[0]-b[0]))
        cross=np.asarray(cross)
        if not (np.all(cross>1e-14) or np.all(cross< -1e-14)):
            raise ValueError("folded or degenerate geographic grid cells")
        return cropped


def export(figure,root,name,title,description):
    paths=[]
    for suffix in ("svg","pdf","png"):
        path=root/(name+"."+suffix)
        metadata={"Title":title,"Description":description,"Date":None} if suffix=="svg" else {"Title":title}
        if suffix=="pdf": metadata.update(CreationDate=None,ModDate=None)
        figure.savefig(path,dpi=400,facecolor="white",metadata=metadata)
        if suffix=="svg":
            # Keep Matplotlib's vector content and add accessible root labels.
            text=path.read_text(); start=text.index('>',text.index('<svg'))+1
            import html
            text=text[:start]+'\n<title>'+html.escape(title)+'</title>\n<desc>'+html.escape(description)+'</desc>'+text[start:]
            path.write_text("\n".join(line.rstrip() for line in text.splitlines())+"\n")
        paths.append({"name":path.name,"sha256":checksum(path)})
    plt.close(figure)
    return paths


def render(snapshots,grid,args):
    plt.rcParams.update({"font.family":"DejaVu Sans","font.size":9,"axes.titlesize":11,
                        "axes.labelsize":9,"axes.spines.top":False,"axes.spines.right":False,
                        "svg.fonttype":"none","svg.hashsalt":"wacomm-publication-v1",
                        "pdf.fonttype":42,"savefig.bbox":"tight"})
    root=args.output_dir;root.mkdir(parents=True,exist_ok=True)
    depth_scale=1000 if args.depth_unit=="mm" else 1
    depth_label=f"Depth below model datum ({args.depth_unit})"
    all_depth=np.concatenate([s["points"]["depth"] for s in snapshots])
    low=min(0,np.floor(all_depth.min()/args.depth_bin)*args.depth_bin)
    high=max(args.depth_bin,np.ceil(all_depth.max()/args.depth_bin)*args.depth_bin)
    bins=np.arange(low,high+args.depth_bin*.5,args.depth_bin)
    if len(bins)>10000: raise ValueError("too many depth bins; increase --depth-bin")
    summary=statistics(snapshots,args.source,bins)
    selection=sorted(set([0,len(snapshots)//2,len(snapshots)-1]))
    max_age=max(s["points"]["age"].max() for s in snapshots)/3600
    norm=Normalize(0,max(1,max_age)); cmap="viridis"
    lon,lat,mask,h=grid
    fig,axes=plt.subplots(1,len(selection),figsize=(12.8,4.8),squeeze=False)
    fig.subplots_adjust(left=.07,right=.9,bottom=.22,top=.80,wspace=.25)
    for panel,(ax,index) in enumerate(zip(axes[0],selection)):
        p=snapshots[index]["points"]
        ax.contourf(lon,lat,mask,levels=[-.5,.5,1.5],colors=["#e3ded3","#eff7fa"])
        bath=ax.contour(lon,lat,np.ma.masked_where(mask==0,h),levels=[5,10,20,50,100],colors="#9aaeb7",linewidths=.55)
        ax.clabel(bath,fontsize=7,fmt="%g m")
        ax.contour(lon,lat,mask,levels=[.5],colors="#626963",linewidths=.7)
        order=np.argsort(p["id"])
        scatter=ax.scatter(p["lon"][order],p["lat"][order],c=p["age"][order]/3600,
                           s=1.4,cmap=cmap,norm=norm,alpha=.65,rasterized=True,linewidths=0)
        ax.scatter(*args.source,marker="*",s=90,c="#d84a36",edgecolor="white",linewidth=.6,zorder=10)
        ax.set(xlim=args.extent[:2],ylim=args.extent[2:],xlabel="Longitude (°E)",
               title=f"{chr(97+panel)}  {utc(snapshots[index]['time']):%H:%M} UTC  |  n={len(order):,}")
        if panel==0: ax.set_ylabel("Latitude (°N)")
        ax.set_aspect("equal"); ax.grid(alpha=.15); ax.tick_params(labelsize=8)
        ax.ticklabel_format(useOffset=False)
    cax=fig.add_axes([.92,.29,.013,.39]);fig.colorbar(scatter,cax=cax,label="Particle age (h)")
    fig.suptitle(args.title+" | particle locations",x=.07,y=.97,ha="left",fontsize=16,fontweight="bold")
    fig.text(.07,.88,f"Model result • {utc(snapshots[0]['time']):%d %B %Y} • star: release location",fontsize=10,color="#52616b")
    note="EPSG:4326 Plate Carrée • ROMS wet/dry boundary and bathymetry • particles are not probabilities"
    fig.text(.07,.10,note,fontsize=8)
    fig.text(.07,.06,args.note,fontsize=8)
    description=(note+f". Extent {args.extent} degrees. Grid: {args.grid.name}. Physical snapshots: "+
                 ", ".join(s["utc"] for s in summary)+". "+args.note)
    outputs=export(fig,root,"sarno-maps",args.title+" particle maps",description)

    fig,axes=plt.subplots(1,3,figsize=(12.8,4.8))
    fig.subplots_adjust(left=.07,right=.98,bottom=.24,top=.79,wspace=.36)
    colors=["#00798c","#d17b0f","#653d9a"]
    for color,index in zip(colors,selection):
        axes[0].stairs(np.asarray(summary[index]["depth_counts"])/(np.diff(bins)*depth_scale),bins*depth_scale,orientation="horizontal",
                       color=color,label=utc(snapshots[index]["time"]).strftime("%H:%M"),linewidth=1.5)
    axes[0].invert_yaxis();axes[0].set(xlabel=f"Selected particles per {args.depth_unit}",ylabel=depth_label,title="a  Vertical distribution")
    axes[0].legend(title="UTC",frameon=False,fontsize=8)
    final=snapshots[-1]["points"]
    distances=np.array([haversine(args.source[1],args.source[0],a,b) for a,b in zip(final["lat"],final["lon"])])/1000
    section=axes[1].scatter(distances,final["depth"]*depth_scale,c=final["age"]/3600,s=1.5,cmap=cmap,norm=norm,alpha=.55,rasterized=True,linewidths=0)
    fig.colorbar(section,ax=axes[1],label="Particle age (h)",fraction=.045,pad=.025)
    axes[1].invert_yaxis();axes[1].set(xlabel="Radial distance from source (km)",ylabel=depth_label,title="b  Final radial section")
    times=[utc(s["time"]) for s in snapshots]
    q=np.array([s["depth_quantiles_m"] for s in summary])*depth_scale
    axes[2].plot(times,q[:,1],"o",color=colors[0],label="Median")
    axes[2].vlines(times,q[:,0],q[:,2],color=colors[0],alpha=.7,label="10–90% member range")
    axes[2].invert_yaxis(); axes[2].set(xlabel="Snapshot time (UTC)",ylabel=depth_label,title="c  Depth summary")
    import matplotlib.dates as mdates
    axes[2].xaxis.set_major_formatter(mdates.DateFormatter("%H:%M"));axes[2].legend(frameon=False,fontsize=8)
    for ax in axes:ax.grid(alpha=.18);ax.tick_params(labelsize=8)
    fig.suptitle(args.title+" | vertical structure",x=.07,y=.97,ha="left",fontsize=16,fontweight="bold")
    fig.text(.07,.88,f"Model result • bins {args.depth_bin:g} m • final section: {utc(snapshots[-1]['time']):%H:%M} UTC",fontsize=10,color="#52616b")
    fig.text(.07,.11,"Unweighted active members • depth = −stored positive-up coordinate • quantile range is not a confidence interval",fontsize=8)
    fig.text(.07,.06,args.note,fontsize=8)
    outputs+=export(fig,root,"sarno-profiles",args.title+" depth profiles",
                    "Vertical count density, final radial depth section, and snapshot depth quantiles. "+description)
    report={"schema":"wacomm-publication-v1","title":args.title,"map_crs":args.map_crs,"extent_degrees":args.extent,
            "note":args.note,"source_lon_lat":args.source,"depth_bin_edges_m":bins.tolist(),"depth_display_unit":args.depth_unit,
            "selection":"finite unmasked geographic coordinates, depth and age; age >= 0; health > 0",
            "interpretation":"unweighted descriptive members, not mass, probabilities or confidence intervals",
            "grid":{"name":args.grid.name,"sha256":checksum(args.grid),"coastline":"ROMS wet/dry mask only"},
            "inputs":[s["input"] for s in snapshots],"statistics":summary,"outputs":outputs,
            "software":{"python":platform.python_version(),"numpy":np.__version__,"matplotlib":matplotlib.__version__,
                        "netCDF4":netCDF4.__version__,"script_sha256":checksum(pathlib.Path(__file__)),
                        "diagnostic_helpers_sha256":checksum(pathlib.Path(__file__).with_name("trajectory_diagnostics.py"))}}
    (root/"sarno-figure-manifest.json").write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
    return report


def main():
    parser=argparse.ArgumentParser(description="Read-only publication maps and particle depth profiles")
    parser.add_argument("snapshots",type=pathlib.Path,nargs="+")
    parser.add_argument("--grid",type=pathlib.Path,required=True)
    parser.add_argument("--map-crs",choices=["EPSG:4326"],required=True)
    parser.add_argument("--extent",type=float,nargs=4,required=True,metavar=("WEST","EAST","SOUTH","NORTH"))
    parser.add_argument("--source",type=float,nargs=2,required=True,metavar=("LON","LAT"))
    parser.add_argument("--depth-bin",type=float,default=.5,help="bin width in metres")
    parser.add_argument("--depth-unit",choices=["m","mm"],default="m")
    parser.add_argument("--title",default="Sarno River • six-hour release")
    parser.add_argument("--note",default="")
    parser.add_argument("--output-dir",type=pathlib.Path,required=True)
    args=parser.parse_args()
    try:
        w,e,s,n=args.extent
        if not all(np.isfinite(args.extent+args.source)) or not (-180<=w<e<=180 and -90<s<n<90) or e-w>10 or n-s>10:
            raise ValueError("require a finite regional extent of at most 10° without antimeridian crossing")
        if not (w<=args.source[0]<=e and s<=args.source[1]<=n): raise ValueError("source must lie inside the extent")
        if not np.isfinite(args.depth_bin) or args.depth_bin<=0: raise ValueError("depth-bin must be positive and finite")
        # Reject output paths that coincide with a simulation input.
        targets={args.output_dir.resolve()/(stem+suffix) for stem in ("sarno-maps","sarno-profiles") for suffix in (".svg",".pdf",".png")}
        targets.add(args.output_dir.resolve()/"sarno-figure-manifest.json")
        if any(p.resolve() in {t.resolve() for t in targets} for p in args.snapshots+[args.grid]): raise ValueError("output would overwrite an input")
        snapshots=read_particles(args.snapshots)
        if any(np.any((p["lon"]<w)|(p["lon"]>e)|(p["lat"]<s)|(p["lat"]>n)) for p in [x["points"] for x in snapshots]):
            raise ValueError("extent would clip selected particles; enlarge it")
        render(snapshots,read_grid(args.grid,args.extent),args)
    except (ValueError,OSError,RuntimeError) as error:
        parser.exit(2,str(error)+"\n")


if __name__=="__main__":
    main()
