#!/bin/bash
set -euo pipefail

repository=$(cd "$(dirname "$0")/.." && pwd)
root="$repository/data/wacomm-sarno-lite"
suite="$root/cuda-scaling"
if [ "$#" -ne 0 ]; then echo "Usage: bash tools/run_sarno_cuda_scaling.sh" >&2; exit 1; fi
for option in USE_MPI:BOOL=ON USE_OMP:BOOL=ON USE_CUDA:BOOL=ON CMAKE_BUILD_TYPE:STRING=Release; do
    if ! grep -qx "$option" "$repository/build-cuda/CMakeCache.txt"; then
        echo "Build requires $option" >&2; exit 1
    fi
done
if [ -e "$suite" ]; then echo "Archive $suite before repeating the experiment." >&2; exit 1; fi
test "$(cat "$root/preparation/provenance/exit.txt")" = 0
(cd "$root" && sha256sum -c preparation/provenance/prepared-forcing.sha256)
mkdir -p "$suite/provenance"
cp "$repository/build-cuda/CMakeCache.txt" "$suite/provenance/"
cp "$repository/build-cuda/CMakeFiles/wacommplusplus.dir/flags.make" "$suite/provenance/"
git -C "$repository" rev-parse HEAD > "$suite/provenance/revision.txt"
git -C "$repository" diff HEAD > "$suite/provenance/working-tree.patch"
cp "$repository/tools/sarno_cuda_scaling_job.sh" "$repository/tools/run_sarno_cuda_scaling.sh" "$suite/provenance/"
cp "$root/preparation/provenance/prepared-forcing.sha256" "$suite/provenance/forcing.sha256"
previous=""
for processes in 1 2 4; do
    run="$suite/p$processes"
    mkdir -p "$run"/{provenance,examples,output-6h,snapshots-6h}
    ln -s ../../processed-6h "$run/processed-6h"
    cp "$repository/build-cuda/wacommplusplus" "$run/"
    cp "$root/scaling-64/p1/wacomm-sarno-lite-6h.json" "$run/"
    mkdir -p "$run/examples/sources-sarno_river"
    cp "$repository/examples/sources-sarno_river/sources-sarno_river.json" "$run/examples/sources-sarno_river/"
    cp "$repository/tools/sarno_cuda_scaling_job.sh" "$run/submit.sh"
    (cd "$run" && sha256sum wacommplusplus wacomm-sarno-lite-6h.json examples/sources-sarno_river/sources-sarno_river.json submit.sh) > "$run/provenance/inputs.sha256"
    nodes=$(( (processes+3)/4 ))
    dependency=()
    if [ -n "$previous" ]; then dependency=(--dependency="afterok:$previous"); fi
    job=$(sbatch --parsable --partition=low-gn --nodes="$nodes" --ntasks="$processes" \
        --cpus-per-task=1 --gres=gpu:tesla:4 --exclusive --mem=0 --time=01:00:00 \
        --job-name="sarno-cuda-p$processes" --chdir="$run" \
        --output="$run/run.out" --error="$run/run.err" "${dependency[@]}" "$run/submit.sh")
    previous="${job%%;*}"
    printf '%s\n' "$previous" > "$run/provenance/job-id.txt"
    printf '%s %s\n' "$processes" "$previous" | tee -a "$suite/provenance/jobs.txt"
done
