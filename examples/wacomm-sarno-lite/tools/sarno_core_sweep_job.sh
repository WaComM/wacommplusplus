#!/bin/bash -l
#SBATCH --job-name=sarno-core-sweep
#SBATCH --nodes=1
#SBATCH --exclusive
#SBATCH --gres=gpu:1
#SBATCH --time=12:00:00
# One-process Sarno sweep over OpenMP threads pinned one per physical core, with zero or one GPU.
# Usage: sbatch --partition=P [--output=LOG] sarno_core_sweep_job.sh SUITE_ROOT RATE BLOCKS "THREADS" "GPUS"
# Environment: BINARY (executable under test), REPOSITORY (checkout with tools/ and examples/),
# FORCING (directory with processed-6h/ and preparation/provenance/prepared-forcing.sha256),
# PYTHON (interpreter with tools/requirements-figures.txt), optional SWEEP_ENV (file to source for modules),
# optional PRUNE_SNAPSHOTS=1 (delete validated particle snapshots, keeping their checksums).
# Every (threads, GPUs) tuple runs once as a warm-up and once per replicate block; the order rotates
# between blocks. Each sample is validated against the 1/1/0 warm-up, then its gridded output is
# replaced by checksums so that a whole suite fits in a user disk quota.
set -euo pipefail
if [ -n "${SWEEP_ENV:-}" ]; then source "$SWEEP_ENV"; fi
root=$1 rate=$2 blocks=$3 threads=($4) gpus=($5)
binary=${BINARY:?set BINARY} repository=${REPOSITORY:?set REPOSITORY} forcing=${FORCING:?set FORCING} python=${PYTHON:?set PYTHON}
tool=$repository/examples/wacomm-sarno-lite/tools/sarno_core_sweep.py
mkdir -p "$root/provenance"
cd "$root"

# Threads are placed on logical CPUs 0..n-1; refuse a topology where two of them share a physical core.
lscpu -e=CPU,CORE > provenance/cpu-topology.txt
maximum=$(printf '%s\n' "${threads[@]}" | sort -n | tail -1)
distinct=$(awk -v n="$maximum" 'NR>1 && $1<n {print $2}' provenance/cpu-topology.txt | sort -u | wc -l)
if [ "$distinct" -ne "$maximum" ]; then
    echo "logical CPUs 0..$((maximum-1)) do not map to $maximum distinct physical cores" >&2; exit 1
fi
if [ ! -f provenance/config.json ]; then
    cp "$binary" provenance/wacommplusplus
    cp "$(dirname "$binary")/CMakeCache.txt" provenance/
    git -C "$(dirname "$binary")/.." rev-parse HEAD > provenance/binary-revision.txt
    git -C "$(dirname "$binary")/.." diff HEAD > provenance/binary-working-tree.patch
    echo "$rate" > provenance/particles-per-hour.txt
    "$python" -I - "$repository/examples/sources-sarno_river/sources-sarno_river.json" provenance/source.json "$rate" <<'PYTHON'
import json,sys
source=json.load(open(sys.argv[1]))
if len(source['features'])!=1: raise ValueError('Sarno sweep requires one declared source')
source['features'][0]['properties']['emission']={'mode':'uniform_rate','rate':int(sys.argv[3]),'rate_unit':'particles/hour'}
open(sys.argv[2],'w').write(json.dumps(source,indent=2)+'\n')
PYTHON
    "$python" -I - "$repository/examples/wacomm-sarno-lite/wacomm-sarno-lite.json" provenance/config.json <<'PYTHON'
import json,sys
config=json.load(open(sys.argv[1]))
config['io'].update(ocean_model='WaComM',base_path='processed-6h/',save_input=False,
                    nc_inputs=['ocm3_d03_20210701Z'+h+'.nc' for h in ('09','10','11','13','14','15')])
open(sys.argv[2],'w').write(json.dumps(config,indent=2)+'\n')
PYTHON
    lscpu > provenance/cpu.txt
    nvidia-smi -L > provenance/gpus.txt
    nvidia-smi -q > provenance/gpu-query.txt
    (module list) > provenance/modules.txt 2>&1 || true
    (cd "$forcing" && sha256sum -c preparation/provenance/prepared-forcing.sha256) > provenance/forcing-check.txt
    cp "$forcing/preparation/provenance/prepared-forcing.sha256" provenance/forcing.sha256
fi
cmp -s "$binary" provenance/wacommplusplus || { echo "binary differs from the suite binary" >&2; exit 1; }
configs=()
for g in "${gpus[@]}"; do for n in "${threads[@]}"; do configs+=("$n:$g"); done; done

run_one() {
    local n=$1 g=$2 label=$3
    local sample=$root/p1_n${n}_g${g}/sample-$label reference=$root/p1_n1_g0/sample-warmup
    if [ -e "$sample/verdict.txt" ] && grep -q '^equivalence_passed=true' "$sample/verdict.txt"; then return; fi
    rm -rf "$sample"
    mkdir -p "$sample"/{examples/sources-sarno_river,output-6h,snapshots-6h}
    ln -s "$forcing/processed-6h" "$sample/processed-6h"
    cp provenance/config.json "$sample/wacomm-sarno-lite-6h.json"
    cp provenance/source.json "$sample/examples/sources-sarno_river/sources-sarno_river.json"
    local devices=-1
    if [ "$g" -gt 0 ]; then devices=$(seq -s, 0 $((g-1))); fi
    date -u +%FT%TZ > "$sample/start.txt"
    set +e
    (cd "$sample" && env OMP_NUM_THREADS="$n" OMP_PLACES="{0}:$n" OMP_PROC_BIND=close OMP_DYNAMIC=false \
        OMP_DISPLAY_AFFINITY=true CUDA_VISIBLE_DEVICES="$devices" \
        /usr/bin/time -f "elapsed_seconds=%e maxrss_kb=%M" -o time.txt \
        "$binary" wacomm-sarno-lite-6h.json > stdout.txt 2> stderr.txt)
    echo $? > "$sample/exit.txt"
    set -e
    date -u +%FT%TZ > "$sample/end.txt"
    local verdict=skipped
    if [ "$(cat "$sample/exit.txt")" = 0 ]; then
        (cd "$sample" && sha256sum output-6h/*.nc snapshots-6h/*.nc > outputs.sha256)
        verdict=$("$python" -I "$tool" validate "$reference" "$sample" "$g" || true)
        if [ "$sample" != "$reference" ]; then
            rm -rf "$sample/output-6h"
            # Large releases: keep only checksums of validated particle snapshots as well.
            if [ "${PRUNE_SNAPSHOTS:-0}" = 1 ] && grep -q '^equivalence_passed=true' <<< "$verdict"; then rm -rf "$sample/snapshots-6h"; fi
        fi
    fi
    echo "$verdict" > "$sample/verdict.txt"
    echo "$(date -u +%T) n=$n g=$g $label exit=$(cat "$sample/exit.txt") $(tail -1 "$sample/time.txt") $verdict"
}

# The 1/1/0 warm-up is the numerical reference for every other sample.
run_one 1 0 warmup
grep -q '^equivalence_passed=true' "$root/p1_n1_g0/sample-warmup/verdict.txt" || { echo "reference run failed" >&2; exit 1; }
for c in "${configs[@]}"; do IFS=: read -r n g <<< "$c"; run_one "$n" "$g" warmup; done
count=${#configs[@]}
for ((b=1; b<=blocks; b++)); do
    offset=$(( (b-1)*5 % count ))
    for ((i=0; i<count; i++)); do
        IFS=: read -r n g <<< "${configs[$(( (i+offset)%count ))]}"
        run_one "$n" "$g" "$b"
    done
done
echo DONE
