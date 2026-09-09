# WaComM++ documentation

WaComM++ is a Lagrangian particle model driven by gridded ocean circulation and, for configured surface objects, 10 m wind. The documentation is part of the same versioned scientific product as the physics, implementation, tests, and examples.

The documentation is organized as a scientific model description rather than solely as software instructions. Equations define sign conventions and units; configuration pages map those equations to reproducible controls; adapter pages state normalization assumptions; and every example identifies its scientific question, validation criterion, inferential limits, and provenance record. Normative scientific references are restricted to peer-reviewed journal articles or peer-reviewed proceedings.

- [Model](model.md)
- [Architecture](architecture.md)
- [Build](build.md)
- [Configuration](configuration.md)
- [Adapters](adapters.md)
- [Backtracking](backtracking.md)
- [Surface drift objects](sar-drift.md)
- [Restart](restart.md)
- [Parallelism](parallelism.md)
- [Testing](testing.md)
- [Reproducibility](reproducibility.md)
- [Examples](examples.md)
- [Supported platforms](supported-platforms.md)
- [Rocky Linux 8.9](rocky-linux.md)
- [RISC-V 64 Linux](riscv64-linux.md)
- [Scientific product policy](versioned-scientific-product.md)
- [References](references.md)

## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Sandve, G. K., Nekrutenko, A., Taylor, J., and Hovig, E. (2013). Ten simple rules for reproducible computational research. *PLoS Computational Biology*, 9, e1003285. [doi:10.1371/journal.pcbi.1003285](https://doi.org/10.1371/journal.pcbi.1003285).
