#!/bin/bash -l
set -euo pipefail

# Continue an existing CPU suite only after its scheduler dependency succeeds.
repository="${1:?repository path required}"
suite_name="${2:?suite name required}"
phase="${3:?gpu or publish required}"
python="${4:?scientific Python path required}"
partition="${5:-low-gn}"
if [[ ! "$suite_name" =~ ^[a-zA-Z0-9-]+$ ]] || [[ "$phase" != gpu && "$phase" != publish ]]; then
    echo 'Expected repository, suite-name, gpu|publish, Python path, and optional GPU partition' >&2
    exit 1
fi
example="$repository/examples/webinar-native-usecase"
suite="$example/data/$suite_name"
cd "$repository"
sha256sum -c "$suite/provenance/continuation/tools.sha256"
module load gcc-12.2.1/ompi-4.1.4_nccl
export WEBINAR_PYTHON="$python"
mkdir -p "$suite/provenance/continuation-$SLURM_JOB_ID"
scontrol show job "$SLURM_JOB_ID" > "$suite/provenance/continuation-$SLURM_JOB_ID/slurm.txt"
"$python" -m pip freeze > "$suite/provenance/continuation-$SLURM_JOB_ID/python-packages.txt"
if [ "$phase" = gpu ]; then
    # The launcher independently revalidates every CPU point before selection.
    bash "$example/tools/run_webinar_protocol.sh" gpu "$suite_name" "$partition"
    last_job=$(tail -n 1 "$suite/provenance/jobs.txt" | awk '{print $3}')
    [[ "$last_job" =~ ^[0-9]+$ ]]
    job=$(sbatch --parsable --partition=low-wn --nodes=1 --ntasks=1 --cpus-per-task=1 \
        --mem=64G --time=24:00:00 --dependency="afterok:$last_job" --kill-on-invalid-dep=yes \
        --job-name=webinar-publish --output="$suite/collect-final.out" --error="$suite/collect-final.err" \
        "$suite/provenance/continuation/webinar_protocol_finish.sh" \
        "$repository" "$suite_name" publish "$python" "$partition")
    printf '%s\n' "$job" > "$suite/provenance/final-collector-job.txt"
else
    "$python" "$example/tools/webinar_protocol_collect.py" "$suite"
    "$python" "$repository/tools/performance_publish.py" "$suite" "$suite/publication" \
        --physical-window '2026-09-15T00:00:00Z/2026-09-16T00:00:00Z'
fi
