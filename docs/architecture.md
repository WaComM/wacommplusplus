# Architecture

`Config` parses runtime policy. `WacommPlusPlus` traverses forcing files and selects explicit ocean, weather, and wave adapters. `OceanModelAdapter`, `WeatherModelAdapter`, and `WaveModelAdapter` normalize product-specific coordinates and fields without changing temporal direction. `Wacomm` coordinates forcing records, sources, decomposition, particle execution, concentration, and output. `Particle` contains the reference CPU numerical update. The CUDA kernel implements the same physical-time interpolation, shortened steps, direction policy, closures, grid metrics, vertical indexing, and counter-key stochastic specification; backend code changes scheduling and memory placement, not the physical model. Dynamic weather and wave arrays currently fail cleanly on CUDA rather than selecting different physics.

The execution hierarchy is forcing progression, MPI/FlexMPI decomposition, OpenMP particle parallelism, and optional CUDA execution. Backend work distribution may differ, but equations and closures may not.

Forcing progression keeps the current normalized adapter plus one adjacent adapter long enough to copy a single boundary record. Forward traversal appends the next file's first record; backward traversal prepends the older file's last record. Compatibility checks cover dimensions, sigma coordinates, longitude, latitude, bathymetry, and mask before any dynamic array is indexed.

Surface drift follows the same hierarchy. `DriftObjectType` and `DriftSide` are fixed-width particle state; `DriftObjectCatalog` is the single coefficient source; and the allocation-free leeway function is shared by CPU and CUDA code. `Config` selects the model and environmental provider before integration. Passive runs follow a configuration-level fast path and do not incur catalog lookup or wind computation.

## References

- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
