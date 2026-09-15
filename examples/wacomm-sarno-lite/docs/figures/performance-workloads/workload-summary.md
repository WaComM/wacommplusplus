# Measured workload-dependent resource choices

Each row reports the fastest validated CPU tuple at that emission rate. The GPU comparison uses the listed one-node CPU reference with zero through four devices at fixed MPI/OpenMP placement. Times are medians of the independent solver-duration sums; these are observed choices on the recorded hardware, not universal optima.

| Particles/hour | Selected CPU (p/n/0) | GPU reference (p/n/0) | CPU median (s) | CPU speedup | CPU efficiency | Best measured (p/n/g) | Best median (s) |
| ---: | :---: | :---: | ---: | ---: | ---: | :---: | ---: |
| 1000 | 1/32/0 | 1/32/0 | 0.0802318 | 18.973 | 0.593 | 1/32/0 | 0.0802318 |
| 10000 | 1/32/0 | 1/32/0 | 0.519478 | 20.094 | 0.628 | 1/32/0 | 0.519478 |
| 100000 | 64/1/0 | 2/16/0 | 3.61455 | 27.981 | 0.437 | 2/16/1 | 2.52182 |
| 1e+06 | 64/1/0 | 2/16/0 | 26.9513 | 36.999 | 0.578 | 2/16/1 | 6.29173 |

A device count is favored only when its measured median is lower at the same emission rate; differences from three repetitions are descriptive and do not establish statistical significance. Examine the per-case speedup and efficiency charts, raw samples, validation reports, and device telemetry before attributing a bottleneck or generalizing to other hardware.
