# PLAN.md — anim-retarget

Goal: a C99 command-line tool that retargets every animation track of a
source GLB (the Universal Animation Library export) onto the armature of
`test/data/test_player.glb`, writing one model-less GLB per track that
plays in the engine exactly like `test/data/sword_run.glb` does.

Specs: `docs/cli.md` (commands), `docs/glb-subset.md` (file format +
fixture facts), `docs/retargeting.md` (math), `docs/libraries.md`
(cgltf / cglm / json-c and what each one does for us). Conventions:
`docs/development-guidelines.md`, `docs/testing-guidelines.md`,
`docs/cest-reference.md`.

## Commit protocol (every phase, no exceptions)

1. Write the tests for the phase first, then the code, until
   `make test` is **all green** (unit + acceptance, ASan on).
2. `git add` only the phase's files; commit with an imperative one-line
   subject (`Add GLB container reader`) and **no `Co-Authored-By`
   trailer** — the user authors the commits alone.
3. Stop after the commit and end the turn with a one-line status so the
   user can run `/usage` and decide whether the next phase fits in the
   remaining budget. Never start the next phase in the same turn.

## Known blocker (resolve before phase 9)

`test/data/UAL1_Standard_RM.glb` is currently a near-duplicate of
`test_player.glb` (same Mixamo rig, one 2-key placeholder track), not the
animation library — see `docs/glb-subset.md` §5. Phases 0–8 need only the
other two fixtures plus synthetic transforms built in tests. The user must
drop in the real export before phase 9.

## Phases

Each phase is one commit. Module names are the `inc/<name>.h` /
`src/<name>.c` pair; tests are `test/unit/<name-with-dashes>.test.cpp`.

### Phase 0 — Scaffold
- Submodules are already added (`third_party/cgltf` v1.15, `cglm` v0.9.6,
  `json-c` json-c-0.19-20260627). `scripts/init.sh`: `git submodule
  update --init`, then Cest header + runner (sha256 table), verify.
- `CMakeLists.txt`: json-c cache options + `add_subdirectory`
  (`libraries.md`); tool target `anim-retarget` (C99, `-Wall -Wextra
  -Werror`, include `inc/`, `third_party/cgltf`,
  `third_party/cglm/include`, link `json-c m`); test targets per
  `testing-guidelines.md`. `Makefile` (`init`, `all`, `test`, `clean`,
  `ensure-init`). `.gitignore` already has `/build/`, `/external/`, `/out/`.
- `inc/error_code.h` with the codes in `docs/cli.md`;
  `inc/app_main.h` + `src/app.c` (`App_Run`: prints usage, returns
  `ERR_BAD_ARGS`; `App_SetOutputStream`), `src/main.c` pass-through.
- Tests: `app.test.cpp` — no args → `ERR_BAD_ARGS`; `--help` → 0; plus
  one smoke `it` that calls `glm_quat_identity` and one that creates and
  frees a `json_object`, proving all three libraries compile and link
  into the test binary.
- Done when `make test` runs green end to end on this machine.

### Phase 1 — `transform`
- Thin layer over cglm: `Transform` struct (`vec3 translation; versor
  rotation; vec3 scale;`), `Transform_Identity`, `Transform_Compose`,
  `Transform_Inverse`, `Transform_ToMat4`, `Transform_FromMat4` (decompose
  + shear check → `ErrorCode`), `Transform_TransformPoint`,
  `Transform_MinimalArc(vec3 from, vec3 to, versor dest)` (wraps
  `glm_quat_from_vecs`, anti-parallel safe), `Transform_EulerDegrees`,
  `Transform_QuatMakeContinuous(previous, current)`.
- Tests: known-answer cases from `testing-guidelines.md`.

### Phase 2 — `gltf_doc` and the `info` command
- Wrapper over cgltf (`CGLTF_IMPLEMENTATION` lives here): load path →
  parse + load buffers + validate, `cgltf_result` → `ErrorCode`; typed
  accessors for nodes (name, parent, children, rest `Transform` via
  `Transform_FromNode`, mesh/skin flags), skins (joints, IBMs as
  `float[16]`), animations (channels → node/path/sampler; samplers →
  interpolation, input/output floats via `cgltf_accessor_unpack_floats`),
  armature root discovery (`glb-subset.md` §3).
- `App_Run("info", path, ["--rest"])` prints the listing in `docs/cli.md`.
- Tests: `gltf-doc.test.cpp` facts (25 joints, IBM accessor 48 / 76,
  sword_run 21 keys 0.0333→0.7, STEP/LINEAR per channel kind, error
  cases); `test/acceptance/info.test.cpp` on both working fixtures via
  the output stream seam.

### Phase 3 — `skeleton` + `anim_track`
- `Skeleton_FromDoc(doc, skin_index)`: joints in depth-first order,
  parent index, node index, name, rest local/global `Transform`,
  `Skeleton_FindJoint(name)`, child iteration.
- `AnimTrack_FromDoc(doc, animation_index, skeleton)`: per-joint channel
  lookup, `AnimTrack_Fps/Start/End`, `AnimTrack_EvaluateLocal(joint, t)`
  for STEP/LINEAR (`glm_quat_slerp`)/CUBICSPLINE, `Pose` (array of local
  transforms) + `Pose_ComputeGlobals(skeleton)`.
- Tests: hierarchy/global facts (cross-check globals against
  `cgltf_node_transform_world` on the rest pose); interpolation
  at/between/outside keys; sword_run grid = 21 frames @ 30 fps.

### Phase 4 — `bone_map`
- Parse `--map` strings and map files; resolve to joint indices on both
  skeletons; validate injectivity; root-pair detection; mapped-child
  lookup; report unmapped lists.
- Tests: syntax cases, errors naming the bad bone, identity map on the
  Mixamo rig resolves all 25.

### Phase 5 — `frame_align` + rest correction
- Horn/Umeyama similarity solve with an in-house Jacobi 4×4 symmetric
  eigen solver (`double`); residual; degenerate fallback.
  `RestAlign_Compute(map, skeletons, Q)` producing `A_s` per mapped pair
  (`retargeting.md` §4) with `Transform_MinimalArc`.
- Tests: parametrized recovery of known transforms; rest correction
  brings `Q·dir_s` onto `dir_d`; leaves/forks inherit.

### Phase 6 — `retarget`
- `Retarget_Create(source skel, dest skel, map, options)`; per-frame
  `Retarget_Frame(source pose, t) → dest local pose` implementing
  `retargeting.md` §5–§7, including `--in-place` and sign-continuous
  quaternions; non-finite guard → `ERR_INTERNAL`.
- Tests: identity-is-identity; synthetic 90°/×0.01 frame change undone;
  synthetic A-pose arm correction; in-place strips horizontal travel.

### Phase 7 — `glb_writer` + the `convert` command
- Build the output document with json-c (`glb-subset.md` §4): node
  subtree copy minus mesh nodes with dense renumbering, skin, animation
  channels/samplers, accessors with input `min`/`max`, bufferViews, BIN
  assembly; serialise `JSON_C_TO_STRING_PLAIN`; frame the GLB in-house
  (header + padded chunks) through the `file_io` write seam.
- `App_Run("convert", …)` end to end; per-track summary; output naming.
- Tests: `glb-writer.test.cpp` shape checks + parse-back with cgltf +
  `cgltf_validate`; `test/acceptance/convert.test.cpp` identity and
  sword_run → test_player cases; malformed input case.

### Phase 8 — CLI completeness and robustness
- `args` module covering every flag in `docs/cli.md` (`--all-anims`,
  `--map-file`, `--fps`, `--src-up`, overrides, `--verbose`, `#index`
  track selection, `--out` single-file rule); usage text; exit codes.
- Synthetic frame-change acceptance test; multiple-track output test on
  a synthetic two-track GLB built in the test (written with
  `glb_writer`, read back with `gltf_doc`).
- `README.md` (what/why/build/run, link to docs).

### Phase 9 — The real library (needs the real fixture)
- User replaces `test/data/UAL1_Standard_RM.glb`; run `info`, fix the
  left-hand names in `docs/mappings/ual-to-mixamo.map`, record the
  measured facts in `docs/glb-subset.md` §5.
- Convert a locomotion track (`--in-place`) and an upper-body track; load
  them in the engine next to `sword_run.glb`. Check: facing, up axis,
  scale of hips motion, arm rest (A/T), foot contact, loop seam, start
  time 0 vs 1/30.
- Fix what the engine reveals (likely candidates: `--src-up` default,
  time offset, twist on hands/feet, `k` from Procrustes vs leg length);
  add an acceptance test on the real fixture: `--all-anims` yields N
  files, each loads, no non-finite values, hips stay within the
  destination's height range.

### Phase 10 — Polish (optional)
- `--bone-offset d=X,Y,Z` manual twist fix if phase 9 needs it.
- Optional Khronos glTF-Validator download in `init.sh` and an
  acceptance gate running it on every produced file when present.

## Decisions already made (don't relitigate)

- cgltf reads, json-c writes, cglm does the math; all three vendored as
  pinned submodules in `third_party/` (`libraries.md`). `cgltf_write.h`
  is not used. In-house code is limited to the GLB framing on write, the
  Horn/Jacobi solve, keyframe sampling and the retargeting itself.
- Output mirrors `sword_run.glb`: model-less, destination armature copied
  verbatim, T/R LINEAR + S STEP for every joint, time grid from 0.
- Retarget in world space with Procrustes frame alignment and minimal-arc
  rest correction; only root pairs get translation; scale never
  retargeted.
- Fixtures live in `test/data/` and are committed.
- Cest v5 fetched by `scripts/init.sh`; one test binary per `*.test.cpp`;
  `cest-runner build/` is the test command.
