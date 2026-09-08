# Physical and numerical model

A particle state is the fractional grid position `(k,j,i)`, health, age, emission time, and stable 64-bit identity. Resolved velocity supplied by an ocean adapter advances position through the local horizontal great-circle metrics and the normalized vertical coordinate. Dynamic fields are sampled on the adapter's common particle grid. Horizontal, upper, and lower closure modes are constraint, kill, or reflection.

The configured `dti` is the maximum integration step in seconds. Each interval uses adjacent physical `ocean_time` values and a shortened final step lands on the interval boundary. Deterministic forward motion has positive time orientation; deterministic backward motion reverses resolved velocity and terminal settling/rise displacement. Diffusion is a zero-mean stochastic displacement. A shortened step scales stochastic amplitude by the square root of elapsed time so variance remains proportional to time.

The physical-time interpolation weight between forcing records is `alpha=(t-T0)/(T1-T0)`, clamped to `[0,1]`; dynamic `u`, `v`, `w`, `zeta`, and `AKT` use that bracket, while static grid data do not. Irregular forcing intervals therefore retain their actual timestamps.

Backward deterministic integration can test trajectory reversibility when diffusion, decay, sources, and non-reversible boundary interactions are disabled. Stochastic backtracking is not an inverse random path; it represents candidate origins conditional on circulation data and modeling assumptions. Closures, finite resolution, missing forcing variables, interpolation, and numerical precision limit interpretation.
