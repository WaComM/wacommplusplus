#!/bin/bash
set -euo pipefail

# Stage the six-hour example below the repository and submit a serial Slurm job.
repository=$(cd "$(dirname "$0")/.." && pwd)
run="$repository/data/wacomm-sarno-lite"
mkdir -p "$run/roms" "$run/processed-6h" "$run/output-6h" "$run/snapshots-6h" "$run/examples" "$run/provenance"
if compgen -G "$run/snapshots-6h/*.nc" > /dev/null || compgen -G "$run/output-6h/*.nc" > /dev/null; then
    echo "Archive existing six-hour outputs before submitting another run." >&2
    exit 1
fi
for hour in 09 10 11 13 14 15; do
    file="rms3_d03_20210701Z${hour}00.nc"
    if [ ! -s "$run/roms/$file" ]; then
        curl --fail --location --retry 2 --output "$run/roms/$file.part" \
            "https://data.meteo.uniparthenope.it/files/rms3/d03/history/2021/07/01/$file"
        mv "$run/roms/$file.part" "$run/roms/$file"
    fi
    ncdump -h "$run/roms/$file" > "$run/provenance/$file.header.txt"
done
cp "$repository/examples/wacomm-sarno-lite.json" "$run/wacomm-sarno-lite-6h.json"
cp "$repository/examples/sources-sarno_river.json" "$run/examples/"
cp "$repository/build/wacommplusplus" "$run/wacommplusplus"
cp "$repository/tools/sarno_lite_job.sh" "$run/submit-6h.sh"
cp "$repository/build/CMakeCache.txt" "$run/provenance/"
git -C "$repository" rev-parse HEAD > "$run/provenance/revision.txt"
git -C "$repository" diff HEAD > "$run/provenance/working-tree.patch"
sbatch --parsable --partition=high-wn --nodes=1 --ntasks=1 --cpus-per-task=1 \
    --mem=32G --time=01:00:00 --job-name=wacomm-sarno-lite \
    --chdir="$run" --output="$run/six-hour-%j.out" --error="$run/six-hour-%j.err" \
    "$run/submit-6h.sh" | tee "$run/provenance/six-hour-job-id.txt"
