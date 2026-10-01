# anim-retarget

A C99 command-line tool that retargets skeletal animation from one GLB
onto the armature of another, even when bone names, rest pose, up axis,
facing and scale all differ. Each source track becomes its own
**model-less GLB** holding the destination armature and one animation,
in the same shape as an animation file the engine already loads.

The motivating case: the Universal Animation Library (`UAL1_Standard_RM.glb`,
43 tracks on a 65-joint UE-Mannequin rig, metres, +Y up) onto a 25-joint
Mixamo rig (`test_player.glb`, centimetre-ish, −Z up).

## How it works

The retargeting happens in world space:

1. **Sampling.** Each source track is sampled on a uniform grid
   (STEP / LINEAR / CUBICSPLINE).
2. **Frame alignment.** A Horn/Umeyama similarity solve over the mapped
   joints' rest positions finds the rotation `Q` and scale `k` from the
   source world to the destination world.
3. **Rest correction.** Each bone gets a minimal-arc correction for the
   difference between the two rest poses (for example A-pose vs T-pose).
4. **Rotations.** Per-bone world rotation deltas are moved into the
   destination frame. Only the hips get translation, and it can be made
   in-place.

The math is specified in [docs/retargeting.md](docs/retargeting.md).

## Build

You need a C/C++ toolchain, CMake ≥ 3.22, git and curl.

```
make init      # submodules (cgltf, cglm, json-c) + Cest test framework
make           # build/anim-retarget
make test      # unit + acceptance tests under ASan
```

## Run

```
build/anim-retarget info test/data/UAL1_Standard_RM.glb

build/anim-retarget convert test/data/UAL1_Standard_RM.glb test/data/test_player.glb \
  --anim Jog_Fwd_Loop,Walk_Loop --map-file docs/mappings/ual-to-mixamo.map \
  --out-dir out/ual --in-place --no-rest-align

build/anim-retarget convert test/data/UAL1_Standard_RM.glb test/data/test_player.glb \
  --all-anims --map-file docs/mappings/ual-to-mixamo.map --out-dir out/ual --no-rest-align
```

`--no-rest-align` is recommended for the UAL → Mixamo pair because both
rigs rest in T-pose (see the phase 9 findings in `PLAN.md`).

Every flag is described in [docs/cli.md](docs/cli.md). Bone maps live in
[docs/mappings/](docs/mappings/).

## Documentation

| Doc | Covers |
|-----|--------|
| [PLAN.md](PLAN.md) | Phases and in-engine validation notes |
| [docs/cli.md](docs/cli.md) | Commands, flags, output, exit codes |
| [docs/retargeting.md](docs/retargeting.md) | The retargeting math |
| [docs/glb-subset.md](docs/glb-subset.md) | What is read and written; fixture facts |
| [docs/libraries.md](docs/libraries.md) | cgltf, cglm, json-c and what each does |
| [docs/development-guidelines.md](docs/development-guidelines.md) | Code conventions |
| [docs/testing-guidelines.md](docs/testing-guidelines.md) | Test layout and expectations |
