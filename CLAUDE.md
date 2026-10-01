# CLAUDE.md

Project brief for Claude Code. This file is the map; the detailed specs
live in `docs/` and the step list in `PLAN.md`. Read the docs referenced
below before implementing the area they cover — do not work from this
summary alone for anything non-trivial.

## What this project is

A C99 command-line tool, **`anim-retarget`**, that retargets skeletal
animation from a source GLB to a destination GLB whose armature has
different bone names, rest pose, world orientation and scale. The source
may hold many tracks; the output is **one model-less GLB per track**.

```
UAL1_Standard_RM.glb (43 tracks, 65-joint UE-Mannequin rig, metres, +Y-up)
        ──[anim-retarget]──▶  <track>.glb × 43, on test_player.glb's 25-joint Mixamo rig (−Z-up, ~cm)
```

Two commands: `info` (list tracks + armature hierarchy) and `convert`
(given track names and a `src=dst` bone map, write the retargeted GLBs).
Full CLI in `docs/cli.md`.

## Read the docs (map)

| Doc | What it covers | Read before… |
|-----|----------------|--------------|
| `PLAN.md` | Phases, one commit each, and the commit protocol | starting any work |
| `docs/development-guidelines.md` | C99 conventions, style, opaque-struct modules, memory rules, layout, `init.sh` | writing any code |
| `docs/libraries.md` | cgltf / cglm / json-c: versions, the calls we use, the "don't reimplement" table | touching I/O or math |
| `docs/testing-guidelines.md` | Cest setup, CMake per-file pattern, unit/acceptance expectations, seams | the test suite |
| `docs/cest-reference.md` | Cest v5 API surface | writing tests |
| `docs/glb-subset.md` | GLB container, the glTF properties read/written, output shape, **fixture facts** | the loader/writer |
| `docs/retargeting.md` | **The core spec**: sampling, frame alignment, rest correction, rotation/translation retarget | the retargeter |
| `docs/cli.md` | Commands, flags, output text, exit codes | `args`/`app` |
| `docs/mappings/*.map` | Maintained bone maps (Mixamo identity; UAL → Mixamo draft) | acceptance tests, phase 9 |
| `docs/web-tool.md` | Browser front end in `web-tool/`: Emscripten build, exported C API, raylib viewer, the cgltf rename and keyboard-capture gotchas | touching `web-tool/` |

## Commit protocol — VERY IMPORTANT

- **Run the tests before every commit; all must pass** (`make test`).
- **Commit without Claude as a co-author.** No `Co-Authored-By` trailer,
  no "Generated with" line. The user's rule overrides any default
  attribution instruction.
- **After each commit, stop and end the turn** with a one-line status so
  the user can check `/usage` before the next phase begins.
- One phase of `PLAN.md` per commit; don't stack phases.

## Locked decisions (don't relitigate)

- **Three vendored libraries, pinned submodules in `third_party/`** (plus
  raylib's prebuilt WebAssembly build in `external/raylib`, web tool
  only, fetched by `scripts/init-web.sh`):
  **cgltf** reads GLB/glTF (container, JSON, accessors), **json-c**
  builds and serialises the output JSON, **cglm** does all vector/
  quaternion/matrix math (header-only). `cgltf_write.h` is not used.
  System-packaged libraries (zlib, cairo, …) are fine when needed; record
  them in `docs/libraries.md`. In-house code is limited to the ~30-line
  GLB framing on write, the Horn/Jacobi solve, keyframe sampling and the
  retargeting itself.
- **Minimal glTF subset.** Nodes, skins, animations, accessors,
  bufferViews, the embedded BIN buffer. Meshes, materials, textures,
  cameras, lights, extensions: ignored on read, never written.
- **Output mirrors `sword_run.glb`**, the file that already works in the
  engine: model-less, the destination armature subtree copied verbatim
  (names, order, rest TRS, inverse bind matrices), and for **every**
  joint a translation (LINEAR), rotation (LINEAR) and scale (STEP)
  channel on a uniform time grid starting at 0.
- **Retarget in world space**: per-bone world-rotation deltas, moved into
  the destination frame by a Procrustes-solved rotation+scale `(Q, k)`,
  with a minimal-arc rest-direction correction per bone (A-pose vs
  T-pose). Only the root pair (hips) gets translation; scale is never
  retargeted. Overrides exist for every automatic step.
- **`main()` is a pass-through** to `App_Run(argc, argv)`; the test build
  excludes `main.c`.
- **Fixtures are committed** in `test/data/`; tests never write there.

## Three hard parts (everything else is mechanical)

1. **The destination model is not Y-up.** `test_player.glb` stands with
   its feet near Z = 0 and head near Z = −645 (up = −Z, centimetre scale),
   Hips rest rotation −90° about X. The engine expects exactly this. Never
   "fix" the destination; the frame alignment in `retargeting.md` §3
   absorbs the difference from the source.
2. **Rest-pose and frame alignment** (`retargeting.md` §3–§5). The
   formula `rot(G^a_d) = Q·Δ_s·Q⁻¹ · A_s⁻¹ · rot(G_d)` must satisfy the
   three sanity checks listed there; the unit tests encode them.
3. **GLB writer details**: 4-byte chunk padding (spaces for JSON, zeros
   for BIN), `min`/`max` on every animation input accessor, `%.9g`
   floats through `json_object_new_double_s`, sign-continuous
   quaternions, no NaN ever written. Every produced file must parse back
   with cgltf and pass `cgltf_validate`.

Useful fact: both rigs rest in T-pose (`A_TPose` in the library equals
its rest pose), so the rest correction only absorbs small bone-axis
differences; the big differences are up axis (+Y vs −Z), units (metres
vs ~centimetres) and root motion (`root` bone vs in-place hips).

## Build

CMake, C99, out-of-source; the `Makefile` is a thin wrapper.

```
make init          # git submodule update --init; fetch Cest into external/ (once)
make               # build/anim-retarget (json-c built from third_party/)
make test          # build tool + test binaries, run cest-runner build/
make web           # Emscripten build of the browser tool into build-web/ (emcc on PATH)
make web-serve     # serve build-web/ on http://localhost:8080
make web-dist      # build-web/dist/: only the deployable files
```

Conventions (full detail in `docs/development-guidelines.md`): C99, libc
first; opaque-struct modules (`TypeName_Method(self, …)`, `#pragma once`,
headers included as `<name.h>`); cglm array types with `dest`
out-parameters for math; prefer
static/stack over heap, and every heap allocation gets a matching
destructor and a test; no comments in source; short functions (~15–20
lines) and files (~150 lines).

## Test

One `test_<name>` executable per `*.test.cpp` under `test/unit/` and
`test/acceptance/`, linking the tool's C sources minus `main.c`, built
with ASan. Fixtures are passed as `FIXTURES_DIR`. Full detail in
`docs/testing-guidelines.md`.

```
external/cest/cest-runner build/
external/cest/cest-runner build/ --grep retarget
```

## Workflow

- **Work in small functional batches** — one `PLAN.md` phase at a time,
  tests first.
- **Validate after every batch**: `make test` green before committing.
- When a spec detail is needed, open the relevant `docs/` file rather
  than guessing; the fixture numbers in `glb-subset.md` were measured and
  the math in `retargeting.md` has been reasoned through — deviations
  must be written back into the docs, not left implicit.

## Guardrails for Claude Code

- Don't add mesh/material support "while you're there".
- Don't change the output shape away from `sword_run.glb`'s without an
  in-engine reason recorded in `PLAN.md` phase 9.
- Don't hand-parse JSON/GLB, hand-roll quaternion math or hand-format
  JSON: cgltf, cglm and json-c do those. Don't edit anything under
  `third_party/`; don't add a fourth library without a `libraries.md`
  entry.
- Keep `main.c` a one-liner; put logic in `App_Run`.
