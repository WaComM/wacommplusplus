import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile


with tempfile.TemporaryDirectory() as directory:
    root=pathlib.Path(directory)
    forcing=root/"forcing.nc"; forcing.write_bytes(b"forcing-data")
    output=root/"output.nc"; output.write_bytes(b"output-data")
    config=root/"config.json"
    config.write_text(json.dumps({"io":{"base_path":str(root),"nc_inputs":["forcing.nc"]},
                                  "sources":{"active":False},"restart":{"active":False}}))
    build=root/"build"; build.mkdir()
    (build/"CMakeCache.txt").write_text("CMAKE_BUILD_TYPE:STRING=Release\nUSE_MPI:BOOL=ON\n")
    manifest=root/"manifest.json"
    subprocess.run([sys.executable,sys.argv[1],"--config",str(config),"--build-dir",str(build),
                    "--source-dir",str(root),"--output",str(output),"--absolute-tolerance","1e-10",
                    "--relative-tolerance","1e-8","--manifest",str(manifest)],check=True)
    data=json.loads(manifest.read_text())
    assert data["schema"]=="wacomm-reproducibility-manifest-v1"
    assert data["build"]["cmake_cache"]["USE_MPI"]=="ON"
    assert data["tolerances"]=={"absolute":1e-10,"relative":1e-8}
    records={record["role"]:record for record in data["files"]}
    assert records["forcing"]["sha256"]==hashlib.sha256(b"forcing-data").hexdigest()
    assert records["output"]["sha256"]==hashlib.sha256(b"output-data").hexdigest()
