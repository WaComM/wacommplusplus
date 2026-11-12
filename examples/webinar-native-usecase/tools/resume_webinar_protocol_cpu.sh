#!/bin/bash
set -euo pipefail

# Resume a rotated CPU suite after preserving a failed or cancelled job.
example=$(cd "$(dirname "$0")/.." && pwd)
repository=$(cd "$example/../.." && pwd)
root="$example/data"
suite_name="${1:-}"
partition="${2:-norm-gn}"
if [[ ! "$suite_name" =~ ^[a-zA-Z0-9-]+$ ]] || [ "$#" -gt 2 ]; then
    echo 'Usage: bash examples/webinar-native-usecase/tools/resume_webinar_protocol_cpu.sh suite-name [partition]' >&2
    exit 1
fi
suite="$root/$suite_name"
test -f "$suite/provenance/config.json"
cp "$example/tools/webinar_protocol_job.sh" "$suite/provenance/webinar_protocol_job-resume.sh"
cp "$example/tools/resume_webinar_protocol_cpu.sh" "$suite/provenance/"
tuples=(1:1:0 2:1:0 4:1:0 8:1:0 16:1:0 32:1:0 64:1:0
        1:2:0 1:4:0 1:8:0 1:16:0 1:32:0
        2:16:0 4:8:0 8:4:0 16:2:0)
previous=""
for repetition in 1 2 3; do
    count="${#tuples[@]}"
    offset=$(( (repetition-1)*5 % count ))
    for ((index=0; index<count; index++)); do
        tuple="${tuples[$(( (index+offset)%count ))]}"
        IFS=: read -r processes threads gpus <<< "$tuple"
        run="$suite/p${processes}_n${threads}_g0"
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
        workers=$((processes*threads))
        worker_nodes=$(((workers+31)/32))
        rank_nodes=1
        if [ "$processes" -gt 16 ]; then rank_nodes=$(((processes+15)/16)); fi
        nodes="$worker_nodes"
        if [ "$rank_nodes" -gt "$nodes" ]; then nodes="$rank_nodes"; fi
        ranks_per_node=32
        if [ "$processes" -gt 16 ]; then ranks_per_node=16; fi
        dependency=()
        if [ -n "$previous" ]; then dependency=(--dependency="afterok:$previous"); fi
        excluded_nodes=()
        if [ -n "${WEBINAR_EXCLUDE_NODES:-}" ]; then
            excluded_nodes=(--exclude="$WEBINAR_EXCLUDE_NODES")
        fi
        job=$(sbatch --parsable --partition="$partition" --nodes="$nodes" --ntasks="$processes" \
            --cpus-per-task="$threads" --exclusive --mem=0 --time="${WEBINAR_TIME_LIMIT:-24:00:00}" \
            --export="ALL,WEBINAR_PROCESSES=$processes,WEBINAR_THREADS=$threads,WEBINAR_GPUS=0,WEBINAR_REPETITION=$repetition,WEBINAR_RANKS_PER_NODE=$ranks_per_node" \
            "${dependency[@]}" "${excluded_nodes[@]}" --job-name="webinar-$tuple-r$repetition-resume" \
            --chdir="$run" --output="$run/job-$repetition-resume.out" \
            --error="$run/job-$repetition-resume.err" "$run/submit.sh")
        previous="${job%%;*}"
        printf '%s\n' "$previous" > "$run/provenance/job-id-$repetition-resume.txt"
        printf '%s %s %s resume\n' "$tuple" "$repetition" "$previous" | tee -a "$suite/provenance/jobs.txt"
    done
done
