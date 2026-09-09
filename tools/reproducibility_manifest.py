#!/usr/bin/env python3

import argparse
import datetime
import hashlib
import json
import os
import pathlib
import platform
import subprocess


def checksum(path):
    digest=hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda:stream.read(1024*1024),b""):
            digest.update(block)
    return digest.hexdigest()


def file_record(role,path):
    resolved=path.resolve()
    if not resolved.is_file():
        raise FileNotFoundError(f"{role} file does not exist: {resolved}")
    return {"role":role,"path":str(resolved),"bytes":resolved.stat().st_size,"sha256":checksum(resolved)}


def cmake_cache(build_directory):
    cache=build_directory/"CMakeCache.txt"
    if not cache.is_file():
        raise FileNotFoundError(f"CMake cache does not exist: {cache}")
    selected={}
    prefixes=("BUILD_","CMAKE_BUILD_TYPE","CMAKE_CXX_COMPILER","CMAKE_CXX_COMPILER_VERSION","USE_")
    for line in cache.read_text(encoding="utf-8",errors="replace").splitlines():
        if not line or line.startswith("//") or line.startswith("#") or "=" not in line or ":" not in line:
            continue
        key_type,value=line.split("=",1)
        key=key_type.split(":",1)[0]
        if key.startswith(prefixes):
            selected[key]=value
    return selected


def git_state(source_directory):
    def git(*arguments):
        result=subprocess.run(["git","-C",str(source_directory),*arguments],check=True,
                              text=True,capture_output=True)
        return result.stdout.strip()
    try:
        return {"revision":git("rev-parse","HEAD"),"dirty":bool(git("status","--porcelain"))}
    except (FileNotFoundError,subprocess.CalledProcessError):
        return {"revision":"unknown","dirty":None}


def configured_inputs(configuration,working_directory):
    records=[]
    io=configuration.get("io",{})
    base=pathlib.Path(io.get("base_path","."))
    if not base.is_absolute():
        base=working_directory/base
    for item in io.get("nc_inputs",[]):
        path=pathlib.Path(item)
        records.append(("forcing",path if path.is_absolute() else base/path))
    environment=configuration.get("environment",{})
    for section,role in (("wind","weather_forcing"),("wave","wave_forcing")):
        for item in environment.get(section,{}).get("nc_inputs",[]):
            path=pathlib.Path(item)
            records.append((role,path if path.is_absolute() else base/path))
    sources=configuration.get("sources",{})
    if sources.get("active") and sources.get("sources_file"):
        path=pathlib.Path(sources["sources_file"])
        records.append(("sources",path if path.is_absolute() else working_directory/path))
    restart=configuration.get("restart",{})
    if restart.get("active") and restart.get("restart_file"):
        path=pathlib.Path(restart["restart_file"])
        records.append(("restart",path if path.is_absolute() else working_directory/path))
    return records


def main():
    parser=argparse.ArgumentParser(description="Create a WaComM++ reproducibility manifest")
    parser.add_argument("--config",required=True,type=pathlib.Path)
    parser.add_argument("--build-dir",required=True,type=pathlib.Path)
    parser.add_argument("--source-dir",type=pathlib.Path,default=pathlib.Path.cwd())
    parser.add_argument("--output",action="append",default=[],type=pathlib.Path)
    parser.add_argument("--test-log",type=pathlib.Path)
    parser.add_argument("--absolute-tolerance",required=True,type=float)
    parser.add_argument("--relative-tolerance",required=True,type=float)
    parser.add_argument("--manifest",required=True,type=pathlib.Path)
    arguments=parser.parse_args()

    configuration_path=arguments.config.resolve()
    configuration=json.loads(configuration_path.read_text(encoding="utf-8"))
    working_directory=pathlib.Path.cwd()
    files=[file_record("configuration",configuration_path)]
    files.extend(file_record(role,path) for role,path in configured_inputs(configuration,working_directory))
    files.extend(file_record("output",path) for path in arguments.output)
    if arguments.test_log:
        files.append(file_record("test_log",arguments.test_log))

    parallel={name:os.environ[name] for name in (
        "OMP_NUM_THREADS","OMP_PROC_BIND","OMP_PLACES","OMPI_COMM_WORLD_SIZE","PMI_SIZE",
        "CUDA_VISIBLE_DEVICES") if name in os.environ}
    manifest={
        "schema":"wacomm-reproducibility-manifest-v1",
        "generated_utc":datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "git":git_state(arguments.source_dir.resolve()),
        "configuration":configuration,
        "files":files,
        "build":{"directory":str(arguments.build_dir.resolve()),"cmake_cache":cmake_cache(arguments.build_dir.resolve())},
        "platform":{"system":platform.system(),"release":platform.release(),"machine":platform.machine(),
                    "python":platform.python_version()},
        "parallel":parallel,
        "tolerances":{"absolute":arguments.absolute_tolerance,"relative":arguments.relative_tolerance}
    }
    arguments.manifest.parent.mkdir(parents=True,exist_ok=True)
    arguments.manifest.write_text(json.dumps(manifest,indent=2,sort_keys=True)+"\n",encoding="utf-8")


if __name__=="__main__":
    main()
