# Configuration

Configuration is JSON or the legacy namelist form. New scientific runs should set an explicit seed and direction:

```json
{"physics":{"random":false,"random_seed":5489,"dti":30},"tracking":{"direction":"forward","backward_diffusion":"none"}}
```

`direction` is `forward` or `backward`. Backward diffusion defaults to `none`; `symmetric_stochastic` enables an ensemble and must not be interpreted as exact path reconstruction. Closure values are `constraint`, `kill`, and `reflection`. Unknown direction and backward-diffusion values are errors.
