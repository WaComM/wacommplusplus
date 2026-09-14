#!/bin/bash
set -euo pipefail

# Resume a selected-CPU GPU suite after preserving failed or cancelled jobs.
example=$(cd "$(dirname "$0")/.." && pwd)
repository=$(cd "$example/../.." && pwd)
root="$repository/data/wacomm-sarno-lite"
suite_name="${1:-}"
partition="${2:-norm-gn}"
if [[ ! "$suite_name" =~ ^[a-zA-Z0-9-]+$ ]] || [ "$#" -gt 2 ]; then
    echo 'Usage: bash examples/wacomm-sarno-lite/tools/resume_sarno_protocol_gpu.sh suite-name [partition]' >&2
    exit 1
fi
suite="$root/$suite_name"
test -f "$suite/results.json"
read -r processes threads < <(python3 - "$suite/results.json" <<'PYTHON'
import json,sys
p,n,g=json.load(open(sys.argv[1]))['selected_cpu']
print(p,n)
PYTHON
)
cp "$example/tools/sarno_protocol_job.sh" "$suite/provenance/sarno_protocol_job-gpu-resume.sh"
cp "$example/tools/resume_sarno_protocol_gpu.sh" "$suite/provenance/"
tuples=("$processes:$threads:1" "$processes:$threads:2" "$processes:$threads:3" "$processes:$threads:4")
previous=""
for repetition in 1 2 3; do
    count="${#tuples[@]}"
    offset=$(( (repetition-1)*5 % count ))
    for ((index=0; index<count; index++)); do
        tuple="${tuples[$(( (index+offset)%count ))]}"
        IFS=: read -r processes threads gpus <<< "$tuple"
        run="$suite/p${processes}_n${threads}_g${gpus}"
        sample="$run/sample-$repetition"
        if [ -f "$sample/exit.txt" ] && [ "$(cat "$sample/exit.txt")" = 0 ]; then
            continue
        fi
        for target in "$sample" "$run/sample-warmup"; do
            if [ -f "$target/exit.txt" ] && [ "$(cat "$target/exit.txt")" != 0 ]; then
                failed="$target-failed-$(date +%s)"
                mv "$target" "$failed"
                mkdir -p "$target"/{examples/sources-sarno_river,output-6h,snapshots-6h}
                ln -s "$root/processed-6h" "$target/processed-6h"
                cp "$suite/provenance/config.json" "$target/wacomm-sarno-lite-6h.json"
                cp "$suite/provenance/source.json" "$target/examples/sources-sarno_river/sources-sarno_river.json"
            fi
        done
        cp "$example/tools/sarno_protocol_job.sh" "$run/submit.sh"
        nodes=1
        if [ "$((processes*threads))" -gt 32 ]; then nodes=2; fi
        dependency=()
        if [ -n "$previous" ]; then dependency=(--dependency="afterok:$previous"); fi
        job=$(sbatch --parsable --partition="$partition" --nodes="$nodes" --ntasks="$processes" \
            --cpus-per-task="$threads" --gres="gpu:tesla:$gpus" \
            --exclusive --mem=0 --time=02:00:00 \
            --export="ALL,SARNO_PROCESSES=$processes,SARNO_THREADS=$threads,SARNO_GPUS=$gpus,SARNO_REPETITION=$repetition" \
            "${dependency[@]}" --job-name="sarno-$tuple-r$repetition-resume" \
            --chdir="$run" --output="$run/job-$repetition-resume.out" \
            --error="$run/job-$repetition-resume.err" "$run/submit.sh")
        previous="${job%%;*}"
        printf '%s\n' "$previous" > "$run/provenance/job-id-$repetition-resume.txt"
        printf '%s %s %s resume\n' "$tuple" "$repetition" "$previous" | tee -a "$suite/provenance/jobs.txt"
    done
done
