---
name: ugv-mechanical
description: TRIGGER when discussing the UGV's chassis, frame, dimensions, printed parts, materials, suspension geometry, or the electronics-compartment cooling fan mount — chassis layout, tubing size, build-plate constraints, suspension travel/link angles, PETG/ABS/ASA/nylon-CF material choice.
---

# UGV mechanical layout

## Chassis concept (current, not frozen)

- six wheels, three per side, skid-steer;
- independent motor per wheel;
- approximately 4 kg target vehicle mass;
- aluminum frame using approximately 10x10 mm square tubing;
- printed body and mounting parts;
- approximately 530 mm floor/chassis length (later design iteration);
- approximately 300 mm earlier chassis-width estimate;
- approximately 180 mm earlier chassis-height estimate;
- printer build plate is 256 mm — larger parts must be split;
- planned suspension travel approximately 65-75 mm;
- typical suspension-link angles considered: approximately 15-20 degrees.

**Do not assume the final chassis dimensions are frozen.** Mechanical
dimensions must remain configurable — treat all of the above as current
estimates, not specs to build tooling around.

## Materials

- PETG for ordinary body parts;
- ABS or ASA for more thermally or mechanically critical parts;
- nylon-carbon-fiber material considered for later high-strength parts.

## Cooling fan (mechanical aspect)

One approximately 120 mm fan is planned for the electronics compartment. For
the fan's *control logic* (thresholds, staged/proportional speed, fault
detection), see `ugv-lighting-cooling` — this skill covers only the physical
mounting/airflow consideration.
