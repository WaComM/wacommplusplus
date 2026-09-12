#!/bin/bash
set -euo pipefail

# Run a fixed six-hour workload sequentially on an exclusive Slurm node.
repository=$(cd "$(dirname "$0")/.." && pwd)
root="$repository/data/wacomm-sarno-lite"
suite="$root/openmp-scaling"
partition="${1:-high-wn}"
if [ "$#" -gt 1 ]; then echo "Usage: bash tools/run_sarno_openmp_scaling.sh [partition]" >&2; exit 1; fi
sinfo -h -p "$partition" | grep -q .
cmp "$repository/build/wacommplusplus" "$root/scaling/p1/wacommplusplus"
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
cp "$repository/tools/sarno_openmp_scaling_job.sh" "$suite/provenance/"
cp "$repository/tools/run_sarno_openmp_scaling.sh" "$suite/provenance/"
cp "$root/preparation/provenance/job-id.txt" "$suite/provenance/preparation-job-id.txt"
cp "$root/preparation/provenance/prepared-forcing.sha256" "$suite/provenance/forcing.sha256"
previous=""
for threads in 1 2 4 8 16 32; do
    run="$suite/t$threads"
    mkdir -p "$run"/{provenance,examples,output-6h,snapshots-6h}
    ln -s ../../processed-6h "$run/processed-6h"
    cp "$repository/build/wacommplusplus" "$run/"
    cp "$root/scaling/p1/wacomm-sarno-lite-6h.json" "$run/wacomm-sarno-lite-6h.json"
    cp "$repository/examples/sources-sarno_river.json" "$run/examples/"
    cp "$repository/tools/sarno_openmp_scaling_job.sh" "$run/submit.sh"
    (cd "$run" && sha256sum wacommplusplus wacomm-sarno-lite-6h.json examples/sources-sarno_river.json submit.sh) > "$run/provenance/inputs.sha256"
    dependency=()
    if [ -n "$previous" ]; then dependency=(--dependency="afterany:$previous"); fi
    job=$(sbatch --parsable --partition="$partition" --nodes=1 --ntasks=1 \
        --cpus-per-task="$threads" --exclusive --mem=0 --time=01:00:00 \
        --job-name="sarno-t$threads" --chdir="$run" \
        --output="$run/run.out" --error="$run/run.err" "${dependency[@]}" "$run/submit.sh")
    previous="${job%%;*}"
    printf '%s\n' "$previous" > "$run/provenance/job-id.txt"
    printf '%s %s\n' "$threads" "$previous" | tee -a "$suite/provenance/jobs.txt"
done
