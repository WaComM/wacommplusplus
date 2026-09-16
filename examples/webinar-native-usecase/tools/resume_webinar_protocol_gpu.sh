#!/bin/bash
set -euo pipefail

# Resume a selected-CPU GPU suite after preserving failed or cancelled jobs.
example=$(cd "$(dirname "$0")/.." && pwd)
repository=$(cd "$example/../.." && pwd)
root="$example/data"
suite_name="${1:-}"
partition="${2:-norm-gn}"
if [[ ! "$suite_name" =~ ^[a-zA-Z0-9-]+$ ]] || [ "$#" -gt 2 ]; then
    echo 'Usage: bash examples/webinar-native-usecase/tools/resume_webinar_protocol_gpu.sh suite-name [partition]' >&2
    exit 1
fi
suite="$root/$suite_name"
test -f "$suite/results.json"
read -r processes threads < <(python3 - "$suite/results.json" <<'PYTHON'
import json,sys
p,n,g=json.load(open(sys.argv[1]))['gpu_reference_cpu']
print(p,n)
PYTHON
)
cp "$example/tools/webinar_protocol_job.sh" "$suite/provenance/webinar_protocol_job-gpu-resume.sh"
cp "$example/tools/resume_webinar_protocol_gpu.sh" "$suite/provenance/"
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
            if [ -f "$target/run.out" ] && { [ ! -f "$target/exit.txt" ] || [ "$(cat "$target/exit.txt")" != 0 ]; }; then
                failed="$target-failed-$(date +%s)"
                mv "$target" "$failed"
                mkdir -p "$target"/{examples/sources-webinar,output,restart}
                ln -s "$root/processed" "$target/processed"
                cp "$suite/provenance/config.json" "$target/webinar-native-usecase.json"
                cp "$suite/provenance/source.json" "$target/examples/sources-webinar/sources-webinar.json"
            fi
        done
        cp "$example/tools/webinar_protocol_job.sh" "$run/submit.sh"
        nodes=1
        if [ "$((processes*threads))" -gt 32 ]; then nodes=2; fi
        dependency=()
        if [ -n "$previous" ]; then dependency=(--dependency="afterok:$previous"); fi
        job=$(sbatch --parsable --partition="$partition" --nodes="$nodes" --ntasks="$processes" \
            --cpus-per-task="$threads" --gres="gpu:tesla:$gpus" \
            --exclusive --mem=0 --time="${WEBINAR_TIME_LIMIT:-24:00:00}" \
            --export="ALL,WEBINAR_PROCESSES=$processes,WEBINAR_THREADS=$threads,WEBINAR_GPUS=$gpus,WEBINAR_REPETITION=$repetition" \
            "${dependency[@]}" --job-name="webinar-$tuple-r$repetition-resume" \
            --chdir="$run" --output="$run/job-$repetition-resume.out" \
            --error="$run/job-$repetition-resume.err" "$run/submit.sh")
        previous="${job%%;*}"
        printf '%s\n' "$previous" > "$run/provenance/job-id-$repetition-resume.txt"
        printf '%s %s %s resume\n' "$tuple" "$repetition" "$previous" | tee -a "$suite/provenance/jobs.txt"
    done
done
