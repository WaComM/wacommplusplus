# Architecture

`Config` parses runtime policy. `WacommPlusPlus` traverses forcing files and selects an explicit adapter. An `OceanModelAdapter` normalizes each product. `Wacomm` coordinates forcing records, sources, decomposition, particle execution, concentration, and output. `Particle` contains the reference CPU numerical update. CUDA is used only for configurations implemented by its parity-validated path; the application warns and uses CPU execution for physical-time interpolation, backtracking, or counter-based diffusion that the CUDA path does not yet implement.

The execution hierarchy is forcing progression, MPI/FlexMPI decomposition, OpenMP particle parallelism, and optional CUDA execution. Backend work distribution may differ, but equations and closures may not.
