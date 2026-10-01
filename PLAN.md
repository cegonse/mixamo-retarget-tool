# PLAN.md — anim-retarget

Goal: a C99 command-line tool that retargets every animation track of a
source GLB (the Universal Animation Library export) onto the armature of
`test/data/test_player.glb`, writing one model-less GLB per track that
plays in the engine exactly like `test/data/sword_run.glb` does.

Specs: `docs/cli.md` (commands), `docs/glb-subset.md` (file format +
fixture facts), `docs/retargeting.md` (math). Conventions:
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

## Known blocker (resolve before phase 11)

`test/data/UAL1_Standard_RM.glb` is currently a near-duplicate of
`test_player.glb` (same Mixamo rig, one 2-key placeholder track), not the
animation library — see `docs/glb-subset.md` §5. Phases 0–10 need only the
other two fixtures plus synthetic transforms built in tests. The user must
drop in the real export before phase 11.

## Phases

Each phase is one commit. Module names are the `inc/<name>.h` /
`src/<name>.c` pair; tests are `test/unit/<name-with-dashes>.test.cpp`.

### Phase 0 — Scaffold
- `CMakeLists.txt` (tool target `anim-retarget`, C99, `-Wall -Wextra
  -Werror`, links `m`; test targets per `testing-guidelines.md`),
  `Makefile` (`init`, `all`, `test`, `clean`, `ensure-init`),
  `scripts/init.sh` (Cest header + runner only; sha256 table),
  `.gitignore` (`/build/`, `/external/`, `/out/`).
- `inc/error_code.h` with the codes in `docs/cli.md`;
  `inc/app_main.h` + `src/app.c` (`App_Run`: prints usage, returns
  `ERR_BAD_ARGS`; `App_SetOutputStream`), `src/main.c` pass-through.
- Tests: `app.test.cpp` — no args → `ERR_BAD_ARGS`; `--help` → 0.
- Done when `make test` runs one green suite end to end on this machine.

### Phase 1 — `vec_math`
- `Vec3`, `Quat`, `Mat4`, `Transform` value types. Functions: vec
  add/sub/scale/dot/cross/length/normalize; quat identity/multiply/
  inverse (conjugate of unit)/normalize/rotate-vector/slerp/from-axis-
  angle/from-to (minimal arc, antiparallel safe)/to-euler-degrees;
  `Transform_Compose(parent, child)`, `Transform_Inverse`,
  `Mat4_FromTransform`, `Mat4_Decompose` (with shear check).
- Tests: known-answer cases from `testing-guidelines.md`.

### Phase 2 — `json` + `json_writer`
- DOM parser over a `const char *` buffer: `JsonValue` tree (object,
  array, string, number as `double`, bool, null), `Json_Parse`,
  `Json_Destroy`, `Json_GetMember`, `Json_ArrayLength`, `Json_ArrayAt`,
  `Json_AsDouble`, `Json_AsInt`, `Json_AsString`, `Json_AsBool`. Strings
  unescaped incl. `\uXXXX` → UTF-8. Errors carry byte offset.
- Writer: growable buffer, `JsonWriter_BeginObject/EndObject/
  BeginArray/EndArray/Key/String/Number/Int/Bool`, compact output,
  `%.9g` floats, string escaping. `JsonWriter_Text` + `_Destroy`.
- Tests: per `testing-guidelines.md`; parse each fixture's JSON chunk
  (extract with a tiny test helper reading the GLB header) without error.

### Phase 3 — `glb_file`
- Load path → header check, JSON chunk (NUL-terminated copy), BIN chunk
  pointer/length; `GlbFile_Write(path, json_text, bin, bin_length)` with
  padding; `file_io` seam for writes.
- Tests: three fixtures load; header/length facts from `glb-subset.md`
  §5; malformed cases; write → load round trip byte-equal.

### Phase 4 — `gltf_doc` and the `info` command
- Typed view: nodes (name, children, parent, TRS, mesh/skin flags),
  skins (joints, IBMs), animations (channels → node/path/sampler;
  samplers → interpolation, decoded input floats, decoded output floats
  incl. normalized ints, element size), accessor decoding with stride,
  unsupported-feature errors. Armature root discovery (`glb-subset.md`
  §3).
- `App_Run("info", path, ["--rest"])` prints the listing in `docs/cli.md`.
- Tests: `gltf-doc.test.cpp` facts (25 joints, IBM accessor 48 / 76,
  sword_run 21 keys 0.0333→0.7, STEP/LINEAR per channel kind);
  `test/acceptance/info.test.cpp` on both working fixtures via the output
  stream seam.

### Phase 5 — `skeleton` + `anim_track`
- `Skeleton_FromDoc(doc, skin_index)`: joints in depth-first order,
  parent index, node index, name, rest local/global `Transform`,
  `Skeleton_FindJoint(name)`, child iteration.
- `AnimTrack_FromDoc(doc, animation_index, skeleton)`: per-joint channel
  lookup, `AnimTrack_Fps/Start/End`, `AnimTrack_EvaluateLocal(joint, t)`
  for STEP/LINEAR/CUBICSPLINE, `Pose` (array of local transforms) +
  `Pose_ComputeGlobals(skeleton)`.
- Tests: hierarchy/global facts; interpolation at/between/outside keys;
  sword_run grid = 21 frames @ 30 fps.

### Phase 6 — `bone_map`
- Parse `--map` strings and map files; resolve to joint indices on both
  skeletons; validate injectivity; root-pair detection; mapped-child
  lookup; report unmapped lists.
- Tests: syntax cases, errors naming the bad bone, identity map on the
  Mixamo rig resolves all 25.

### Phase 7 — `frame_align` + rest correction
- Horn/Umeyama similarity solve with a Jacobi 4×4 symmetric eigen
  solver; residual; degenerate fallback. `RestAlign_Compute(map,
  skeletons, Q)` producing `A_s` per mapped pair (`retargeting.md` §4).
- Tests: parametrized recovery of known transforms; rest correction
  brings `Q·dir_s` onto `dir_d`; leaves/forks inherit.

### Phase 8 — `retarget`
- `Retarget_Create(source skel, dest skel, map, options)`; per-frame
  `Retarget_Frame(source pose, t) → dest local pose` implementing
  `retargeting.md` §5–§7, including `--in-place` and sign-continuous
  quaternions; non-finite guard → `ERR_INTERNAL`.
- Tests: identity-is-identity; synthetic 90°/×0.01 frame change undone;
  synthetic A-pose arm correction; in-place strips horizontal travel.

### Phase 9 — `glb_writer` + the `convert` command
- Build the output document (`glb-subset.md` §4): node subtree copy minus
  mesh nodes with dense renumbering, skin, animation channels/samplers,
  accessors with input `min`/`max`, bufferViews, BIN assembly; emit via
  `glb_file`.
- `App_Run("convert", …)` end to end; per-track summary; output naming.
- Tests: `glb-writer.test.cpp` shape checks + load-back;
  `test/acceptance/convert.test.cpp` identity and sword_run → test_player
  cases; malformed input case.

### Phase 10 — CLI completeness and robustness
- `args` module covering every flag in `docs/cli.md` (`--all-anims`,
  `--map-file`, `--fps`, `--src-up`, overrides, `--verbose`, `#index`
  track selection, `--out` single-file rule); usage text; exit codes.
- Synthetic frame-change acceptance test; multiple-track output test on
  a synthetic two-track GLB built in the test.
- `README.md` (what/why/build/run, link to docs).

### Phase 11 — The real library (needs the real fixture)
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

### Phase 12 — Polish (optional)
- `--bone-offset d=X,Y,Z` manual twist fix if phase 11 needs it.
- Optional Khronos glTF-Validator download in `init.sh` and an
  acceptance gate running it on every produced file when present.

## Decisions already made (don't relitigate)

- In-house JSON/GLB/math code, no third-party source
  (`development-guidelines.md`).
- Output mirrors `sword_run.glb`: model-less, destination armature copied
  verbatim, T/R LINEAR + S STEP for every joint, time grid from 0.
- Retarget in world space with Procrustes frame alignment and minimal-arc
  rest correction; only root pairs get translation; scale never
  retargeted.
- Fixtures live in `test/data/` and are committed.
- Cest v5 fetched by `scripts/init.sh`; one test binary per `*.test.cpp`;
  `cest-runner build/` is the test command.
