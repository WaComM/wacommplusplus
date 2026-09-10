# Object and process catalogs

## Object catalog

The public `drift.object_type` names below map to restart-stable unsigned 16-bit identifiers. Identifiers 0--5 retain their earlier meanings. `Source key` identifies the corresponding OBJECTPROP.DAT class distributed with OpenDrift from the USCG/SAROPS lineage; it is provenance, not an alternative configuration spelling. Source slopes in percent are divided by 100, while offsets and residual standard deviations in cm s-1 are divided by 100 before entering the SI equation in [surface drift](sar-drift.md). The implementation preserves separate signed right and left crosswind means and side-specific non-negative residual scales.

| ID | Configuration name | Source key | Distress configuration |
|---:|---|---|---|
| 0 | `PASSIVE` | `PASSIVE` | no wind-relative object motion |
| 1 | `PERSON_IN_WATER` | `PIW-1` | unknown state, mean |
| 2 | `LIFERAFT_NO_DROGUE` | `LIFE-RAFT-NB-4` | no ballast, canopy, no drogue |
| 3 | `LIFERAFT_DROGUE` | `LIFE-RAFT-NB-5` | no ballast, canopy, with drogue |
| 4 | `GENERIC_VESSEL` | `FISHING-VESSEL-1` | fishing-vessel mean |
| 5 | `SHIPPING_CONTAINER` | `CONTAINER-2` | 20-ft, 80% submerged |
| 6 | `PERSON_IN_WATER_PFD` | `PIW-2` | conscious, vertical type-III PFD |
| 7 | `PERSON_IN_WATER_SURVIVAL_SUIT` | `PIW-4` | survival suit, face up |
| 8 | `PERSON_IN_WATER_DECEASED` | `PIW-6` | deceased, face down |
| 9 | `LIFERAFT_DEEP_BALLAST` | `LIFE-RAFT-DB-10` | deep-ballast general mean |
| 10 | `LIFERAFT_DEEP_BALLAST_CAPSIZED` | `LIFE-RAFT-DB-21` | deep-ballast, capsized |
| 11 | `KAYAK_WITH_PERSON` | `PERSON-POWERED-VESSEL-1` | sea kayak, person on aft deck |
| 12 | `SURFBOARD_WITH_PERSON` | `PERSON-POWERED-VESSEL-2` | surfboard with person |
| 13 | `SKIFF` | `SKIFF-1` | modified-V/cathedral-hull skiff |
| 14 | `SKIFF_CAPSIZED` | `SKIFF-3` | swamped or capsized skiff |
| 15 | `SPORT_BOAT` | `SPORT-BOAT` | no canvas, modified-V hull |
| 16 | `COASTAL_FREIGHTER` | `COASTAL-FREIGHTER` | coastal freighter |
| 17 | `SAILBOAT` | `SAILBOAT-1` | monohull mean |
| 18 | `FISHING_VESSEL_DEBRIS` | `FV-DEBRIS` | fishing-vessel debris |
| 19 | `OIL_DRUM` | `OIL-DRUM` | 55-gallon (220 litre) drum |
| 20 | `WWII_MINE` | `MINE` | World War II L-MK2 mine |
| 21 | `REFUGEE_RAFT_NO_SAIL` | `REFUGEE-RAFT-1` | Cuban refugee raft without sail |
| 22 | `MEDICAL_WASTE` | `MED-WASTE-1` | medical-waste mean |

These labels provide leeway only. For example, `OIL_DRUM` does not activate oil weathering and `MEDICAL_WASTE` does not activate degradation, exposure, or dose models. Catalog inclusion is numerical capability, not observational validation for a region, season, forcing product, or individual object.

## Process catalog

The process catalog is an inventory of the single solver's existing operators. It is not a preset system and does not change defaults. A process is active only through the named configuration and required forcing.

| Process | Configuration or forcing | Quantity and units | Stochastic/state contract |
|---|---|---|---|
| `ocean_current` | selected ocean adapter | velocity, m s-1 | deterministic, stateless |
| `leeway` | `drift.model=leeway` | velocity, m s-1 | deterministic mean; fixed random residuals optional |
| `stokes_drift` | `environment.wave.adapter=WW3` with leeway | velocity, m s-1 | deterministic, stateless |
| `horizontal_diffusion` | `physics.random`, `physics.sigma` | displacement, m | stochastic, counter-keyed |
| `vertical_diffusion` | `physics.random` and ocean `AKt` | diffusivity, m2 s-1 | stochastic, counter-keyed displacement |
| `settling_rise` | `physics.sv` | terminal velocity, m s-1 | deterministic, stateless |
| `decay` | `physics.survprob`, `physics.tau0` | e-folding time, s | deterministic concentration update |
| `wind_error` | `environment.wind.uncertainty_stddev` | component error, m s-1 | stochastic per interval/substep |
| `jibing` | `drift.jibe_probability_per_hour` | hourly transition probability, dimensionless | stochastic; resolved side is restart state |

Deterministic velocities are evaluated in physical time and the solver applies tracking direction to their combined displacement. Diffusion in backward runs is disabled by default; the opt-in symmetric stochastic mode is not an inverse realization. Decay is not generally reversible. Jibing transitions are meaningful as a forward stochastic process; backward ensembles remain candidate-origin sensitivity experiments. Continuous and restarted runs preserve the resolved object ID and side, and counter keys reconstruct stateless random terms independently of rank, thread, or accelerator scheduling.

## References

- Breivik, Ø., Allen, A. A., Maisondieu, C., and Roth, J.-C. (2011). Wind-induced drift of objects at sea: the leeway field method. *Applied Ocean Research*, 33, 100--109. [doi:10.1016/j.apor.2011.01.005](https://doi.org/10.1016/j.apor.2011.01.005).
- Breivik, Ø., Allen, A. A., Maisondieu, C., Roth, J.-C., and Forest, B. (2012). The leeway of shipping containers at different immersion levels. *Ocean Dynamics*, 62, 741--752. [doi:10.1007/s10236-012-0522-z](https://doi.org/10.1007/s10236-012-0522-z).
- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405--1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Thygesen, U. H. (2011). How to reverse time in stochastic particle tracking models. *Journal of Marine Systems*, 88, 159--168. [doi:10.1016/j.jmarsys.2011.03.009](https://doi.org/10.1016/j.jmarsys.2011.03.009).
