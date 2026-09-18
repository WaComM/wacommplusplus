#!/bin/bash -l
set -euo pipefail

run_root="$PWD"
trap 'status=$?; mkdir -p "$run_root/provenance/attempt-$SLURM_JOB_ID"; cp "$run_root"/provenance/*.txt "$run_root/provenance/attempt-$SLURM_JOB_ID/"; exit "$status"' EXIT
module load openssl/openssl-4.0.2
module load cmake/cmake-4.4.3
module load gcc-12.2.1/ompi-4.1.4_nccl
module load nvidia/cuda-12.8.0
export OMP_NUM_THREADS="$WEBINAR_THREADS"
export OMP_THREAD_LIMIT="$WEBINAR_THREADS"
export OMP_DYNAMIC=FALSE
export OMP_PROC_BIND=TRUE
export OMP_PLACES=cores
if [ "$WEBINAR_GPUS" -eq 0 ]; then
    export CUDA_VISIBLE_DEVICES=-1
else
    printf '%s\n' "${CUDA_VISIBLE_DEVICES:-}" > provenance/slurm-visible-devices.txt
    IFS=, read -r -a available_devices <<< "${CUDA_VISIBLE_DEVICES:-}"
    if [ "${#available_devices[@]}" -lt "$WEBINAR_GPUS" ]; then
        echo "Slurm exposed fewer than $WEBINAR_GPUS GPU devices" >&2
        exit 1
    fi
    visible_devices=("${available_devices[@]:0:$WEBINAR_GPUS}")
    CUDA_VISIBLE_DEVICES=$(IFS=,; printf '%s' "${visible_devices[*]}")
    export CUDA_VISIBLE_DEVICES
fi
module list > provenance/modules.txt 2>&1
uname -a > provenance/platform.txt
lscpu > provenance/cpu.txt
mpirun --version > provenance/mpi.txt
scontrol show job "$SLURM_JOB_ID" > provenance/slurm.txt
printenv | LC_ALL=C sort | sed -n '/^OMP_\|^CUDA_VISIBLE_DEVICES=/p' > provenance/runtime.txt
nvidia-smi -L > provenance/gpus.txt 2>&1 || true
if [ "$WEBINAR_GPUS" -gt 0 ]; then
    nvidia-smi topo -m > provenance/gpu-topology.txt
    nvidia-smi -q > provenance/gpu-details.txt
fi
if [ "$WEBINAR_REPETITION" -eq 1 ]; then
    repetitions=(warmup 1)
else
    repetitions=("$WEBINAR_REPETITION")
fi
for repetition in "${repetitions[@]}"; do
    sample="sample-$repetition"
    if [ "$repetition" = warmup ] && [ -f "$sample/exit.txt" ] && [ "$(cat "$sample/exit.txt")" = 0 ]; then
        continue
    fi
    if [ -f "$sample/run.out" ]; then echo "Refusing to overwrite $sample" >&2; exit 1; fi
    cd "$sample"
    set +e
    monitor=""
    if [ "$WEBINAR_GPUS" -gt 0 ]; then
        nvidia-smi dmon -s pucm -d 1 -o DT -f gpu-monitor.log &
        monitor=$!
    fi
    mapping="slot:PE=$WEBINAR_THREADS"
    if [ "$WEBINAR_PROCESSES" -gt "${WEBINAR_RANKS_PER_NODE:-32}" ]; then
        mapping="ppr:${WEBINAR_RANKS_PER_NODE}:node:PE=$WEBINAR_THREADS"
    fi
    /usr/bin/time -f '%e' -o elapsed-seconds.txt \
        mpirun --np "$WEBINAR_PROCESSES" --map-by "$mapping" \
        -x CUDA_VISIBLE_DEVICES -x OMP_NUM_THREADS -x OMP_THREAD_LIMIT \
        -x OMP_DYNAMIC -x OMP_PROC_BIND -x OMP_PLACES \
        --bind-to core --report-bindings \
        bash -c 'printf "GPU binding: rank=%s host=%s devices=%s\n" "$OMPI_COMM_WORLD_RANK" "$(hostname)" "$CUDA_VISIBLE_DEVICES"; exec ../wacommplusplus webinar-native-usecase.json' \
        > run.out 2> run.err
    status=$?
    if [ -n "$monitor" ]; then
        kill "$monitor" 2>/dev/null
        wait "$monitor" 2>/dev/null
    fi
    set -e
    printf '%s\n' "$status" > exit.txt
    cd ..
    if [ "$status" -ne 0 ]; then exit "$status"; fi
    find "$sample/output" "$sample/restart" -type f -exec sha256sum {} + \
        > "$sample/outputs.sha256"
done
