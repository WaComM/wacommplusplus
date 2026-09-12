#!/bin/bash
set -euo pipefail

# Rebuild native forcing once, outside the performance experiment.
repository=$(cd "$(dirname "$0")/.." && pwd)
root="$repository/data/wacomm-sarno-lite"
run="$root/preparation"
if [ "$#" -ne 0 ]; then echo "Usage: bash tools/prepare_sarno_scaling.sh" >&2; exit 1; fi
if [ -e "$run" ] || [ -e "$root/processed-6h-before-scaling" ]; then
    echo "Archive the existing preparation run and forcing backup before repeating." >&2; exit 1
fi
for hour in 09 10 11 13 14 15; do
    test -s "$root/roms/rms3_d03_20210701Z${hour}00.nc"
done
for option in USE_MPI:BOOL=ON USE_OMP:BOOL=ON USE_CUDA:BOOL=OFF CMAKE_BUILD_TYPE:STRING=Release; do
    grep -qx "$option" "$repository/build/CMakeCache.txt"
done
mkdir -p "$run"/{provenance,examples,output-6h,snapshots-6h}
if [ -e "$root/processed-6h" ]; then mv "$root/processed-6h" "$root/processed-6h-before-scaling"; fi
mkdir "$root/processed-6h"
ln -s ../roms "$run/roms"
ln -s ../processed-6h "$run/processed-6h"
cp "$repository/build/wacommplusplus" "$run/"
cp "$repository/build/CMakeCache.txt" "$run/provenance/"
cp "$repository/examples/wacomm-sarno-lite/wacomm-sarno-lite.json" "$run/wacomm-sarno-lite-6h.json"
mkdir -p "$run/examples/sources-sarno_river"
cp "$repository/examples/sources-sarno_river/sources-sarno_river.json" "$run/examples/sources-sarno_river/"
cp "$repository/tools/sarno_scaling_job.sh" "$run/submit.sh"
cat >> "$run/submit.sh" <<'SCRIPT'
sha256sum processed-6h/ocm3_d03_20210701Z*.nc > provenance/prepared-forcing.sha256
SCRIPT
git -C "$repository" rev-parse HEAD > "$run/provenance/revision.txt"
git -C "$repository" diff HEAD > "$run/provenance/working-tree.patch"
(cd "$run" && sha256sum wacommplusplus wacomm-sarno-lite-6h.json examples/sources-sarno_river/sources-sarno_river.json roms/rms3_d03_20210701Z*.nc) > "$run/provenance/inputs.sha256"
sbatch --parsable --partition=high-wn --nodes=1 --ntasks=1 --cpus-per-task=1 \
    --exclusive --mem=0 --time=01:00:00 --job-name=sarno-prepare \
    --chdir="$run" --output="$run/run.out" --error="$run/run.err" "$run/submit.sh" \
    | tee "$run/provenance/job-id.txt"
