# Configuration

Configuration is JSON or the legacy namelist form. New scientific runs should set an explicit seed and direction:

```json
{"physics":{"random":false,"random_seed":5489,"dti":30},"tracking":{"direction":"forward","backward_diffusion":"none"}}
```

`direction` is `forward` or `backward`. Backward diffusion defaults to `none`; `symmetric_stochastic` enables an ensemble and must not be interpreted as exact path reconstruction. Closure values are `constraint`, `kill`, and `reflection`. Unknown direction and backward-diffusion values are errors.

`physics.dti` and `physics.deltat` are seconds and must be finite and greater than zero. `physics.sigma` is a stochastic displacement scale in meters and `physics.shore_limit` is a positive-down water-column threshold in meters; both must be finite and non-negative. Invalid numerical or closure values fail during configuration loading rather than entering the integration loop.
