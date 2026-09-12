#!/bin/bash
set -euo pipefail

# Run a fixed six-hour workload sequentially on an exclusive Slurm node.
repository=$(cd "$(dirname "$0")/.." && pwd)
root="$repository/data/wacomm-sarno-lite"
suite_name="${1:-scaling}"
partition="${2:-high-wn}"
if [ "$#" -gt 2 ] || [[ ! "$suite_name" =~ ^[a-zA-Z0-9-]+$ ]]; then echo "Usage: bash tools/run_sarno_scaling.sh [suite-name [partition]]" >&2; exit 1; fi
suite="$root/$suite_name"
for option in USE_MPI:BOOL=ON USE_OMP:BOOL=ON USE_CUDA:BOOL=OFF CMAKE_BUILD_TYPE:STRING=Release; do
    if ! grep -qx "$option" "$repository/build/CMakeCache.txt"; then
        echo "Build requires $option" >&2; exit 1
    fi
done
if [ -e "$suite" ]; then echo "Archive $suite before repeating the experiment." >&2; exit 1; fi
test "$(cat "$root/preparation/provenance/exit.txt")" = 0
(cd "$root" && sha256sum -c preparation/provenance/prepared-forcing.sha256)
for hour in 09 10 11 13 14 15; do
    test -s "$root/processed-6h/ocm3_d03_20210701Z${hour}.nc"
done
mkdir -p "$suite/provenance"
cp "$repository/build/CMakeCache.txt" "$suite/provenance/"
cp "$repository/build/CMakeFiles/wacommplusplus.dir/flags.make" "$suite/provenance/"
git -C "$repository" rev-parse HEAD > "$suite/provenance/revision.txt"
git -C "$repository" diff HEAD > "$suite/provenance/working-tree.patch"
cp "$repository/tools/sarno_scaling_job.sh" "$suite/provenance/"
cp "$repository/tools/run_sarno_scaling.sh" "$suite/provenance/"
cp "$root/preparation/provenance/job-id.txt" "$suite/provenance/preparation-job-id.txt"
cp "$root/preparation/provenance/prepared-forcing.sha256" "$suite/provenance/forcing.sha256"
previous=""
counts=(1 2 4 8 16 32)
if [ "$suite_name" != scaling ]; then counts+=(64); fi
for processes in "${counts[@]}"; do
    run="$suite/p$processes"
    mkdir -p "$run"/{provenance,examples,output-6h,snapshots-6h}
    ln -s ../../processed-6h "$run/processed-6h"
    cp "$repository/build/wacommplusplus" "$run/"
    python3 - "$repository/examples/wacomm-sarno-lite/wacomm-sarno-lite.json" "$run/wacomm-sarno-lite-6h.json" <<'PYTHON'
import json,sys
with open(sys.argv[1]) as stream: config=json.load(stream)
config["io"].update(ocean_model="WaComM",base_path="processed-6h/",save_input=False,
                     nc_inputs=["ocm3_d03_20210701Z"+hour+".nc" for hour in ("09","10","11","13","14","15")])
with open(sys.argv[2],"w") as stream: json.dump(config,stream,indent=2);stream.write("\n")
PYTHON
    mkdir -p "$run/examples/sources-sarno_river"
    cp "$repository/examples/sources-sarno_river/sources-sarno_river.json" "$run/examples/sources-sarno_river/"
    cp "$repository/tools/sarno_scaling_job.sh" "$run/submit.sh"
    (cd "$run" && sha256sum wacommplusplus wacomm-sarno-lite-6h.json examples/sources-sarno_river/sources-sarno_river.json submit.sh) > "$run/provenance/inputs.sha256"
    dependency=()
    if [ -n "$previous" ]; then dependency=(--dependency="afterany:$previous"); fi
    nodes=1
    if [ "$processes" -gt 32 ]; then nodes=2; fi
    job=$(sbatch --parsable --partition="$partition" --nodes="$nodes" --ntasks="$processes" \
        --cpus-per-task=1 --exclusive --mem=0 --time=01:00:00 \
        --job-name="sarno-p$processes" --chdir="$run" \
        --output="$run/run.out" --error="$run/run.err" "${dependency[@]}" "$run/submit.sh")
    previous="${job%%;*}"
    printf '%s\n' "$previous" > "$run/provenance/job-id.txt"
    printf '%s %s\n' "$processes" "$previous" | tee -a "$suite/provenance/jobs.txt"
done
