# Architecture

`Config` parses runtime policy. `WacommPlusPlus` traverses forcing files and selects an explicit adapter. An `OceanModelAdapter` normalizes each product. `Wacomm` coordinates forcing records, sources, decomposition, particle execution, concentration, and output. `Particle` contains the reference CPU numerical update. The CUDA kernel implements the same physical-time interpolation, shortened steps, direction policy, closures, grid metrics, vertical indexing, and counter-key stochastic specification; backend code changes scheduling and memory placement, not the physical model.

The execution hierarchy is forcing progression, MPI/FlexMPI decomposition, OpenMP particle parallelism, and optional CUDA execution. Backend work distribution may differ, but equations and closures may not.

Forcing progression keeps the current normalized adapter plus one adjacent adapter long enough to copy a single boundary record. Forward traversal appends the next file's first record; backward traversal prepends the older file's last record. Compatibility checks cover dimensions, sigma coordinates, longitude, latitude, bathymetry, and mask before any dynamic array is indexed.
