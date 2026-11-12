#!/usr/bin/env python3

import argparse
import json
import pathlib

import netCDF4
import numpy as np

from trajectory_diagnostics import checksum, TIME_UNITS


def snapshot_index(directory):
    result={}
    for path in sorted(directory.glob("*.nc")):
        with netCDF4.Dataset(path) as data:
            if "particle_time" not in data.variables or "id" not in data.variables:
                raise ValueError(f"{path}: requires particle_time and id")
            time=data.variables["particle_time"]
            if getattr(time,"units","")!=TIME_UNITS or getattr(time,"calendar","")!="gregorian":
                raise ValueError(f"{path}: unsupported physical time metadata")
            values=np.ma.asarray(time[:])
            if values.shape!=(1,) or np.ma.getmaskarray(values).any() or not np.isfinite(values[0]):
                raise ValueError(f"{path}: expected one finite particle time")
            value=float(values[0])
            if value in result: raise ValueError(f"{directory}: duplicate physical time")
            result[value]=path
    if not result: raise ValueError(f"{directory}: no particle snapshots")
    return result


def order(data):
    ids=np.ma.asarray(data.variables["id"][:])
    if ids.ndim!=1 or ids.dtype.kind not in "ui" or np.ma.getmaskarray(ids).any():
        raise ValueError("requires unmasked integer particle identifiers")
    if len(np.unique(ids))!=len(ids): raise ValueError("duplicate particle identifiers")
    return np.argsort(ids),len(ids)


def compare(reference,candidate):
    left=snapshot_index(reference);right=snapshot_index(candidate)
    if left.keys()!=right.keys(): raise ValueError("physical snapshot times differ")
    records=[]
    for time in sorted(left):
        with netCDF4.Dataset(left[time]) as a, netCDF4.Dataset(right[time]) as b:
            ai,acount=order(a);bi,bcount=order(b)
            if acount!=bcount: raise ValueError(f"{time}: particle counts differ")
            if set(a.variables)!=set(b.variables): raise ValueError(f"{time}: variable sets differ")
            checked=[]
            for name in sorted(a.variables):
                av=a.variables[name];bv=b.variables[name]
                if av.dimensions!=bv.dimensions or av.dtype!=bv.dtype:
                    raise ValueError(f"{time}: {name} schema differs")
                for attr in ("units","unit","positive","calendar"):
                    if getattr(av,attr,None)!=getattr(bv,attr,None):
                        raise ValueError(f"{time}: {name} {attr} differs")
                x=np.ma.asarray(av[:]);y=np.ma.asarray(bv[:])
                if "particles" in av.dimensions:
                    axis=av.dimensions.index("particles")
                    x=np.take(x,ai,axis=axis);y=np.take(y,bi,axis=axis)
                if x.shape!=y.shape or not np.array_equal(np.ma.getmaskarray(x),np.ma.getmaskarray(y)):
                    raise ValueError(f"{time}: {name} shape or missing-data mask differs")
                xv=x.compressed();yv=y.compressed()
                if xv.dtype.kind in "fc" and (not np.isfinite(xv).all() or not np.isfinite(yv).all()):
                    raise ValueError(f"{time}: {name} contains non-finite unmasked data")
                if not np.array_equal(xv,yv): raise ValueError(f"{time}: {name} values differ")
                checked.append(name)
            records.append({"particle_time":time,"particles":acount,"variables":checked,
                            "reference":{"name":left[time].name,"sha256":checksum(left[time])},
                            "candidate":{"name":right[time].name,"sha256":checksum(right[time])}})
    return {"schema":"wacomm-particle-equivalence-v1","equal":True,"comparison":"exact numeric equality after integer-ID ordering",
            "absolute_tolerance":0,"relative_tolerance":0,"selection":"all stored particles, including inactive; identical masks required",
            "metadata":"variable units, sign, calendar, type and dimensions compared; global build/configuration attributes excluded",
            "snapshots":records}


def main():
    parser=argparse.ArgumentParser(description="Read-only exact comparison of particle snapshots by physical time and integer identity")
    parser.add_argument("reference",type=pathlib.Path)
    parser.add_argument("candidate",type=pathlib.Path)
    parser.add_argument("--json",type=pathlib.Path,required=True)
    args=parser.parse_args()
    try:
        if args.json.resolve() in {p.resolve() for directory in (args.reference,args.candidate) for p in directory.glob("*.nc")}:
            raise ValueError("report would overwrite an input")
        result=compare(args.reference,args.candidate)
        args.json.parent.mkdir(parents=True,exist_ok=True)
        args.json.write_text(json.dumps(result,indent=2,sort_keys=True)+"\n")
    except (ValueError,OSError,RuntimeError) as error:
        parser.exit(2,str(error)+"\n")


if __name__=="__main__":
    main()
