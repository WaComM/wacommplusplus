# Examples

The examples directory contains runnable configurations and explanatory guides. Each configuration is an executable scientific hypothesis: it identifies forcing, initial or terminal conditions, temporal direction, stochastic policy, boundary behavior, object physics where applicable, and output naming. Forcing entries may be local paths or explicit HTTP, HTTPS, or DAP4 locations. Relative ocean, weather, and wave entries inherit `io.base_path` unless the environmental provider declares its own `base_path`. Preserve the full configuration alongside results and follow the validation and reproducibility guidance in each example.

Every JSON file has a same-name Markdown guide. The guide states the scientific objective, prerequisites and required fields, exact command, expected behavior, validation criterion, limitations, interpretation, reproducibility metadata, and peer-reviewed references. The documentation test enforces the JSON-to-guide mapping; scientific review remains responsible for evaluating whether the cited model and forcing assumptions are suitable for a particular experiment.

The paired `sar-person-forward` and `sar-person-backward` examples isolate the catalog-mean leeway term using uniform 10 m wind. `sar-person-ensemble-forward` and `sar-person-ensemble-backward` add fixed, seed-and-identity-keyed regression residuals. `sar-person-side-ensemble-forward` and `sar-person-side-ensemble-backward` instead sample a fixed side from an explicit Bernoulli prior. They are verification and sensitivity scenarios, not operational SAR forecasts or calibrated probability products. The historical files whose names contain `sar` retain their original passive source/restart role unless their JSON explicitly selects `drift.model=leeway`.

## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Sandve, G. K., Nekrutenko, A., Taylor, J., and Hovig, E. (2013). Ten simple rules for reproducible computational research. *PLoS Computational Biology*, 9, e1003285. [doi:10.1371/journal.pcbi.1003285](https://doi.org/10.1371/journal.pcbi.1003285).
