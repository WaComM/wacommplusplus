# Drive the complete wacommplusplus executable on a generated analytic case.
#
# Required variables: HELPER, EXECUTABLE, POLYGON_TOOL, WORK_DIR.
# Optional variables: LAUNCHER, a ;-separated launcher prefix (for example mpiexec -n 2);
# REQUIRE_CUDA_DEVICE, true for CUDA builds, which report a skip without a visible device;
# COLLECTIVE, true for builds with collective output (MPI and parallel NetCDF).

foreach(required HELPER EXECUTABLE POLYGON_TOOL WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

if(REQUIRE_CUDA_DEVICE)
    find_program(NVIDIA_SMI nvidia-smi)
    set(device_status 1)
    if(NVIDIA_SMI)
        execute_process(COMMAND "${NVIDIA_SMI}" -L RESULT_VARIABLE device_status OUTPUT_QUIET ERROR_QUIET)
    endif()
    if(NOT device_status EQUAL 0)
        message("WACOMM_END_TO_END_SKIP: CUDA build without a visible device")
        return()
    endif()
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

function(run_step)
    execute_process(COMMAND ${ARGN}
            WORKING_DIRECTORY "${WORK_DIR}"
            RESULT_VARIABLE status
            OUTPUT_VARIABLE output
            ERROR_VARIABLE output)
    message("${output}")
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "Step failed (${status}): ${ARGN}")
    endif()
endfunction()

run_step("${HELPER}" generate "${WORK_DIR}")
run_step(${LAUNCHER} "${EXECUTABLE}" forward.json)
run_step(${LAUNCHER} "${EXECUTABLE}" backward.json)
run_step(${LAUNCHER} "${EXECUTABLE}" forward-async.json)
run_step(${LAUNCHER} "${EXECUTABLE}" backward-async.json)
run_step(${LAUNCHER} "${EXECUTABLE}" minutes.json)
run_step("${POLYGON_TOOL}" polygon-forward.json polygon.geojson 20210701Z0000)
run_step(${LAUNCHER} "${EXECUTABLE}" polygon-forward.json)
run_step("${POLYGON_TOOL}" polygon-backward.json polygon.geojson 20210701Z0200)
run_step(${LAUNCHER} "${EXECUTABLE}" polygon-backward.json)
set(verify_mode)
if(COLLECTIVE)
    run_step(${LAUNCHER} "${EXECUTABLE}" forward-collective.json)
    run_step(${LAUNCHER} "${EXECUTABLE}" backward-collective.json)
    set(verify_mode collective)
endif()
run_step("${HELPER}" verify "${WORK_DIR}" ${verify_mode})
