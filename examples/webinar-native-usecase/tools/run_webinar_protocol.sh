#!/bin/bash
set -euo pipefail

# Stage the shared Webinar workload and submit serialized, independently archived jobs.
example=$(cd "$(dirname "$0")/.." && pwd)
repository=$(cd "$example/../.." && pwd)
root="$example/data"
phase="${1:-}"
run_name="${2:-}"
partition="${3:-norm-gn}"
particles_per_hour="${4:-250000}"
after_job="${5:-}"
if [[ "$phase" != cpu && "$phase" != gpu ]] || [[ ! "$run_name" =~ ^[a-zA-Z0-9-]+$ ]] ||
   [[ ! "$particles_per_hour" =~ ^[1-9][0-9]*$ ]] ||
   { [ -n "$after_job" ] && [[ ! "$after_job" =~ ^[1-9][0-9]*$ ]]; } || [ "$#" -gt 5 ]; then
    echo 'Usage: bash examples/webinar-native-usecase/tools/run_webinar_protocol.sh cpu|gpu suite-name [partition [particles-per-hour [after-job-id]]]' >&2
    exit 1
fi
suite="$root/$run_name"
if [ "$phase" = cpu ] && [ -e "$suite" ]; then
    echo "Archive existing suite $suite before repeating." >&2; exit 1
fi
if [ "$phase" = gpu ] && [ ! -f "$suite/results.json" ]; then
    echo "Complete and collect the CPU phase before GPU submission." >&2; exit 1
fi
if [ "$phase" = gpu ] && [ "$#" -ge 4 ] &&
   [ "$(cat "$suite/provenance/particles-per-hour.txt")" != "$particles_per_hour" ]; then
    echo "GPU particle count must match the completed CPU suite." >&2; exit 1
fi
if [ ! -f "$root/preparation/provenance/smoke-passed.txt" ]; then
    echo 'Protocol incomplete: prepare and validate the 25 native forcing files before submitting any benchmark jobs.' >&2
    exit 1
fi
if [ "$phase" = gpu ]; then
    "${WEBINAR_PYTHON:-python3.11}" "$example/tools/webinar_protocol_collect.py" "$suite"
fi
for option in USE_MPI:BOOL=ON USE_OMP:BOOL=ON USE_CUDA:BOOL=ON CMAKE_BUILD_TYPE:STRING=Release; do
    grep -qx "$option" "$repository/build-cuda/CMakeCache.txt"
done
(cd "$root" && sha256sum -c preparation/provenance/prepared-forcing.sha256)
if [ "$phase" = cpu ]; then
    mkdir -p "$suite/provenance"
    if [ -n "$after_job" ]; then printf '%s\n' "$after_job" > "$suite/provenance/after-job-id.txt"; fi
    cp "$repository/build-cuda/CMakeCache.txt" "$suite/provenance/"
    cp "$repository/build-cuda/CMakeFiles/wacommplusplus.dir/flags.make" "$suite/provenance/"
    git -C "$repository" rev-parse HEAD > "$suite/provenance/revision.txt"
    git -C "$repository" diff HEAD > "$suite/provenance/working-tree.patch"
    cp -r "$example/tools" "$suite/provenance/tools"
    cp "$repository/docs/performance-evaluation.md" "$suite/provenance/"
    cp "$repository/tools/requirements-figures.txt" "$suite/provenance/"
    (cd "$suite/provenance" && sha256sum performance-evaluation.md) > "$suite/provenance/protocol.sha256"
    cp "$repository/tools/performance_evidence.py" "$repository/tools/performance_protocol.py" "$suite/provenance/"
    cp "$root/preparation/provenance/prepared-forcing.sha256" "$suite/provenance/forcing.sha256"
    printf '%s\n' "$particles_per_hour" > "$suite/provenance/particles-per-hour.txt"
    python3 - "$repository/examples/sources-webinar/sources-webinar.json" "$suite/provenance/source.json" "$particles_per_hour" <<'PYTHON'
import json,sys
with open(sys.argv[1]) as stream: source=json.load(stream)
if len(source['features'])!=1: raise ValueError('Webinar protocol requires one declared source')
source['features'][0]['properties']['emission']={
    'mode':'uniform_rate','rate':int(sys.argv[3]),'rate_unit':'particles/hour'}
with open(sys.argv[2],'w') as stream: json.dump(source,stream,indent=2);stream.write('\n')
PYTHON
    cp "$example/webinar-native-usecase.json" "$suite/provenance/config.json"

else
    if [ -n "$after_job" ]; then printf '%s\n' "$after_job" > "$suite/provenance/gpu-after-job-id.txt"; fi
    cp "$example/tools/run_webinar_protocol.sh" "$suite/provenance/run_webinar_protocol-gpu.sh"
    cp "$example/tools/webinar_protocol_job.sh" "$suite/provenance/webinar_protocol_job-gpu.sh"
fi
if [ "$phase" = cpu ]; then
    tuples=(1:1:0 2:1:0 4:1:0 8:1:0 16:1:0 32:1:0 64:1:0
            1:2:0 1:4:0 1:8:0 1:16:0 1:32:0
            2:16:0 4:8:0 8:4:0 16:2:0)
else
    read -r processes threads < <(python3 - "$suite/results.json" <<'PYTHON'
import json,sys
p,n,g=json.load(open(sys.argv[1]))['gpu_reference_cpu']
print(p,n)
PYTHON
)
    tuples=("$processes:$threads:1" "$processes:$threads:2" "$processes:$threads:3" "$processes:$threads:4")
fi
# Stage all tuples before submission. A run is never overwritten.
for tuple in "${tuples[@]}"; do
    IFS=: read -r processes threads gpus <<< "$tuple"
    run="$suite/p${processes}_n${threads}_g${gpus}"
    if [ -e "$run" ]; then echo "Run exists: $run" >&2; exit 1; fi
    mkdir -p "$run/provenance"
    if [ "$phase" = cpu ]; then
        cp "$repository/build-cuda/wacommplusplus" "$run/"
    else
        cp "$suite/p1_n1_g0/wacommplusplus" "$run/"
    fi
    cp "$example/tools/webinar_protocol_job.sh" "$run/submit.sh"
    for repetition in warmup 1 2 3; do
        sample="$run/sample-$repetition"
        mkdir -p "$sample"/{examples/sources-webinar,output,restart}
        ln -s "$root/processed" "$sample/processed"
        cp "$suite/provenance/config.json" "$sample/webinar-native-usecase.json"
        cp "$suite/provenance/source.json" "$sample/examples/sources-webinar/sources-webinar.json"
    done
    (cd "$run" && sha256sum wacommplusplus sample-1/webinar-native-usecase.json \
        sample-1/examples/sources-webinar/sources-webinar.json submit.sh) > "$run/provenance/inputs.sha256"
done

# Rotate configuration order between independent replicate blocks.
previous="$after_job"
for repetition in 1 2 3; do
    count="${#tuples[@]}"
    offset=$(( (repetition-1)*5 % count ))
    for ((index=0; index<count; index++)); do
        tuple="${tuples[$(( (index+offset)%count ))]}"
        IFS=: read -r processes threads gpus <<< "$tuple"
        run="$suite/p${processes}_n${threads}_g${gpus}"
        nodes=1
        if [ "$((processes*threads))" -gt 32 ]; then nodes=2; fi
        dependency=()
        if [ -n "$previous" ]; then dependency=(--dependency="afterok:$previous"); fi
        gres=()
        if [ "$gpus" -gt 0 ]; then gres=(--gres="gpu:tesla:$gpus"); fi
        job=$(sbatch --parsable --partition="$partition" --nodes="$nodes" --ntasks="$processes" \
            --cpus-per-task="$threads" --exclusive --mem=0 --time="${WEBINAR_TIME_LIMIT:-24:00:00}" \
            --export="ALL,WEBINAR_PROCESSES=$processes,WEBINAR_THREADS=$threads,WEBINAR_GPUS=$gpus,WEBINAR_REPETITION=$repetition" \
            "${gres[@]}" "${dependency[@]}" --job-name="webinar-$tuple-r$repetition" \
            --chdir="$run" --output="$run/job-$repetition.out" --error="$run/job-$repetition.err" "$run/submit.sh")
        previous="${job%%;*}"
        printf '%s\n' "$previous" > "$run/provenance/job-id-$repetition.txt"
        printf '%s %s %s\n' "$tuple" "$repetition" "$previous" | tee -a "$suite/provenance/jobs.txt"
    done
done
