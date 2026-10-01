# GLB / glTF 2.0 Subset

What the tool reads from a GLB, what it writes, and the hard facts about
the three fixtures in `test/data/`. Spec reference:
https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html . Reading is
done by **cgltf**, writing by **json-c** plus ~30 lines of in-house GLB
framing (`libraries.md`). Everything not listed here is **ignored on read
and never written**.

## 1. GLB container

```
offset  size  field
0       4     magic      0x46546C67  ("glTF")
4       4     version    2
8       4     length     total file length
12      …     chunks, each: uint32 chunkLength, uint32 chunkType, data
```
- Chunk 0: type `0x4E4F534A` ("JSON"), UTF-8 JSON text, padded with
  spaces (`0x20`) to a multiple of 4.
- Chunk 1: type `0x004E4942` ("BIN\0"), raw buffer 0, padded with `0x00`
  to a multiple of 4. Optional on read (a JSON-only GLB is valid but useless
  here: error out if animation data is needed and no BIN exists).
- All integers little-endian. All floats IEEE-754 `float32`.
- Read: `cgltf_parse_file` + `cgltf_load_buffers` + `cgltf_validate` do
  all of this; the tool only maps a failing `cgltf_result` to
  `ERR_BAD_GLB` and names it.
- Write (in-house, `glb_writer`): exactly the two chunks above; the JSON
  text comes from json-c (`JSON_C_TO_STRING_PLAIN`); `length` computed
  last.

## 2. glTF properties used (through `cgltf_data`)

| Object | Properties used | Notes |
|--------|-----------------|-------|
| `asset` | `version`, `generator` | `version` must be `"2.0"`; `generator` shown by `info` |
| `scene`, `scenes[].nodes` | root node list | used to find the armature root |
| `nodes[]` | `name`, `children`, `translation`, `rotation`, `scale`, `matrix`, `skin`, `mesh` | `matrix` → decomposed to TRS (error if the 3×3 has shear, i.e. columns not orthogonal within 1e-4); `mesh`/`skin` only used to recognise mesh nodes to skip |
| `skins[]` | `name`, `joints`, `inverseBindMatrices`, `skeleton` | IBMs decoded to 16 floats each (MAT4, column-major) |
| `animations[]` | `name`, `channels[].sampler`, `channels[].target.node`, `channels[].target.path`, `samplers[].input`, `samplers[].output`, `samplers[].interpolation` | paths `translation`/`rotation`/`scale`; `weights` channels ignored with a note |
| `accessors[]` | everything cgltf needs; the tool reads only through `cgltf_accessor_read_float` / `cgltf_accessor_unpack_floats` | strides, normalised ints and sparse accessors are cgltf's job |
| `bufferViews[]`, `buffers[]` | resolved by `cgltf_load_buffers` | external `.bin` and data URIs load too; a GLB with no BIN and no URI fails as `ERR_BAD_GLB` |

Defaults per spec: `translation` `[0,0,0]`, `rotation` `[0,0,0,1]`,
`scale` `[1,1,1]`, `byteOffset` 0, `interpolation` `"LINEAR"`.

Accessor element types the tool expects: `SCALAR` (inputs), `VEC3`
(translation, scale), `VEC4` (rotation), `MAT4` (inverse bind matrices);
anything else on an animation sampler or skin is `ERR_BAD_GLB`.

CUBICSPLINE samplers store 3 output elements per key (in-tangent, value,
out-tangent); see `retargeting.md` §2 for evaluation.

Quaternions are stored `(x, y, z, w)` — the same layout as cglm's
`versor`. Matrices are column-major — the same as cglm's `mat4` and
`cgltf_node_transform_local/world`. The node's local matrix is
`T · R · S`.

## 3. The armature

The **armature** of a file is the node subtree holding the joints of
`skins[0]` (files with more than one skin: use skin 0 and warn). Its
**root node** is the topmost ancestor of the joints that is still reachable
from the scene (in both fixtures that is the `Armature` node, which has
identity TRS). Global transforms include every ancestor up to and including
that root; the scene itself has no transform.

Joint hierarchy order for the tool's own arrays is **depth-first from the
root, parents before children**, which is *not* node-index order (Blender
numbers children before parents: `mixamorig:Hips` is node 24, its child
`Spine` node 13, `HeadTop_End` node 0).

## 4. Output document shape

The output is a **model-less animation GLB**, mirroring `sword_run.glb`,
which is known to load in the engine:

```json
{
  "asset": {"generator": "anim-retarget 0.1", "version": "2.0"},
  "scene": 0,
  "scenes": [{"name": "Scene", "nodes": [<armature root index>]}],
  "nodes": [ ...copied from the destination armature subtree... ],
  "skins": [{"name": "...", "joints": [...], "inverseBindMatrices": <acc>}],
  "animations": [{"name": "<track name>", "channels": [...], "samplers": [...]}],
  "accessors": [...], "bufferViews": [...],
  "buffers": [{"byteLength": N}]
}
```

- `nodes`: the destination armature subtree copied **verbatim** — same
  node indices, names, `children`, rest `translation`/`rotation`/`scale`
  (a `matrix` node is written back as decomposed TRS). Mesh nodes
  (`mesh` present) are dropped from the copy and from their parent's
  `children`; node indices are renumbered densely but joint order is kept.
- `skins[0]`: destination `joints` in the destination's order,
  `inverseBindMatrices` copied as-is (a MAT4 float accessor), `name`
  copied.
- `animations[0]`: name = source track name. For **every joint** three
  channels: `translation` (LINEAR), `rotation` (LINEAR), `scale` (STEP),
  exactly as `sword_run.glb` has 75 channels for 25 joints. One shared
  input accessor for the T/R time grid (N keys, `min`/`max` set), one
  shared 2-key input accessor (first and last time) for the scale
  channels. Output accessors: `VEC3`/`VEC4` float, one bufferView each
  (or one view per channel kind; either is fine, offsets 4-byte aligned).
  As implemented (phase 7): accessor 0 is the N-key time grid, 1 the
  scale time pair (a single key when N = 1), then per skin joint in skin
  order the T, R and S outputs, and last the copied MAT4 IBMs; one
  bufferView per accessor. Channels and samplers are in the same order
  (sampler i ↔ channel i), T/R/S per joint, like `sword_run.glb`.
- **Every animation input accessor carries `min` and `max`** (required by
  the spec). Output accessors need none.
- Time grid starts at `0` and advances by `1/fps` (see `retargeting.md`
  §2). Note: Blender writes the first key at `1/30 s` (frame 1), which the
  engine tolerates; starting at 0 is standard and must be verified
  in-engine once (`PLAN.md` phase 9).
- No `meshes`, `materials`, `textures`, `images`, `samplers`,
  `extensionsUsed`.
- Floats serialised by json-c with `%.9g` (`json_object_new_double_s`,
  round-trip safe for float32); `-0` is fine; `NaN`/`Inf` must never reach
  the writer (assert finite upstream).
- Acceptance check: every produced file parses with cgltf and passes
  `cgltf_validate`.
- Output path: `<out-dir>/<track>.glb` where `<track>` is the source track
  name with every character outside `[A-Za-z0-9._-]` replaced by `_`;
  `--out <file>` overrides when exactly one track is converted.

## 5. Fixture facts (measured 2026-10-01)

All three were exported by "Khronos glTF Blender I/O v5.1.19", glTF 2.0,
one scene, root node `Armature` (identity TRS). Units are centimetres-ish
(Mixamo: `Spine` is 42 units long, `LeftUpLeg` 195).

### `test_player.glb` — the destination model (144,068 bytes)
- 27 nodes: `Armature` (node 26) → `HUmar body` (node 25, `mesh` 0,
  `skin` 0) and `mixamorig:Hips` (node 24) with 24 descendants. 25 joints,
  Mixamo names:
  ```
  mixamorig:Hips
    Spine → Spine1 → Spine2 → { Neck → Head → HeadTop_End,
                                LeftShoulder → LeftArm → LeftForeArm → LeftHand,
                                RightShoulder → RightArm → RightForeArm → RightHand }
    LeftUpLeg → LeftLeg → LeftFoot → LeftToeBase → LeftToe_End
    RightUpLeg → RightLeg → RightFoot → RightToeBase → RightToe_End
  ```
- Skin 0 `"Armature"`: joints in order Hips, Spine, Spine1, Spine2, Neck,
  Head, …; `inverseBindMatrices` accessor 48 (25 × MAT4).
- Hips rest: `T = (-5.53, -10.80, -412.68)`, `R = (-0.7071, 0, 0, 0.7071)`
  (−90° about X). Several joints carry an explicit `scale` of
  `(1, 1, 1)` within float noise.
- Mesh bounds: X −272…259, Y −63…96, Z −646…4. The character's feet are
  near Z = 0 and its head near Z = −645: **the model's up axis in file
  space is −Z and it is ~645 units tall.** This is not glTF-conventional
  Y-up, and it is what the engine expects. The tool must never
  "normalise" it.
- One animation `"mixamo.com"`: 75 channels (T/R/S × 25), all STEP, 2 keys
  at t = 0.0333 and 0.0667 — a rest-pose placeholder, not real motion.
- Also contains 8 materials, 9 textures, 8 PNG images,
  `KHR_materials_transmission`/`KHR_materials_specular` — all ignored.

### `sword_run.glb` — a working in-engine animation (35,560 bytes)
- Model-less: `Armature` (node 25) → `mixamorig:Hips` (node 24) + the same
  24 descendants; same 25 names; `skins[0]` with joints + IBMs (accessor
  76); **no mesh, no material; the `Armature` node has no `skin`.**
- Rest pose is *close to but not identical to* `test_player.glb`: Hips
  rest `T = (0, 0, −413.13)`, shoulders differ by ~2°. The engine therefore
  binds animation to the model by joint name/order and does not require the
  animation file's rest pose or IBMs to equal the model's. The tool still
  copies the destination's values exactly, which is the safest choice.
- One animation `"mixamo.com"`: translation and rotation channels LINEAR
  with 21 keys from t = 0.0333 to 0.7 (30 fps, 21 frames); scale channels
  STEP with 2 keys (first/last). Hips translation keys are absolute in
  `Armature` space, ≈ `(−3.6, 5.4, −388.5)`, first key == last key (loop),
  i.e. **in-place** motion (no root displacement).

### `UAL1_Standard_RM.glb` — the source library (7,620,504 bytes)
Exported by "Khronos glTF Blender I/O v4.5.48"; JSON chunk 2.09 MB, BIN
5.75 MB, 8,090 accessors. Universal Animation Library "Standard" pack
with root motion ("RM").
- 67 nodes: `Armature` (node 66, identity) → `Mannequin` (node 65,
  `mesh` 0, `skin` 0) and `root` (node 64, `R = (−0.7071, 0, 0, 0.7071)`,
  no translation) with 64 descendants. **65 joints**, UE5-Mannequin
  names, skin joint order starting `root, pelvis, spine_01, spine_02,
  spine_03, neck_01, Head, clavicle_l, …`; `inverseBindMatrices`
  accessor 14.
  ```
  root
    pelvis
      spine_01 → spine_02 → spine_03 → { neck_01 → Head,
                                         clavicle_l → upperarm_l → lowerarm_l → hand_l → 5 fingers × (01, 02, 03, 04_leaf),
                                         clavicle_r → … (mirror) }
      thigh_l → calf_l → foot_l → ball_l → ball_leaf_l
      thigh_r → calf_r → foot_r → ball_r → ball_leaf_r
  ```
  Note the capital `Head` and that the spine has three segments
  (`spine_01..03`), one neck segment and 20 finger joints the destination
  lacks.
- Units are **metres**, glTF **+Y-up**: mesh bounds X ±0.972,
  Y 0.0005…1.829, Z −0.164…0.205 — a 1.83 m character in **T-pose**
  (arm span 1.94 m). `pelvis` rest `T = (0, 0.050, 0.917)` in `root`
  space, i.e. world height 0.917 m. The `root` bone's −90° X rotation is
  Blender's Z-up → Y-up conversion baked into the rig.
- The rest pose **is** the T-pose: the track `A_TPose` holds the rest
  rotations on every frame (verified on `upperarm_l`). Both rigs
  therefore rest in T-pose; the rest-direction correction of
  `retargeting.md` §4 only has to absorb small bone-axis differences.
- 43 animations, every one with translation + rotation + scale LINEAR
  channels for all 65 joints (195 channels), **30 fps, first key at
  t = 0**, uniform 1/30 s spacing. Durations 0.167 s (`Pistol_Aim_*`) to
  5.2 s (`Fixing_Kneeling`). Names: `A_TPose`, `Crouch_Fwd_Loop`,
  `Crouch_Idle_Loop`, `Dance_Loop`, `Death01`, `Driving_Loop`,
  `Fixing_Kneeling`, `Hit_Chest`, `Hit_Head`, `Idle_Loop`,
  `Idle_Talking_Loop`, `Idle_Torch_Loop`, `Interact`, `Jog_Fwd_Loop`,
  `Jump_Land`, `Jump_Loop`, `Jump_Start`, `PickUp_Table`,
  `Pistol_Aim_Down/Neutral/Up`, `Pistol_Idle_Loop`, `Pistol_Reload`,
  `Pistol_Shoot`, `Punch_Cross`, `Punch_Jab`, `Push_Loop`, `Roll`,
  `Sitting_Enter/Exit/Idle_Loop/Talking_Loop`,
  `Spell_Simple_Enter/Exit/Idle_Loop/Shoot`, `Sprint_Loop`,
  `Swim_Fwd_Loop`, `Swim_Idle_Loop`, `Sword_Attack`, `Sword_Idle`,
  `Walk_Formal_Loop`, `Walk_Loop`.
- **Root motion** lives in the `root` bone's translation (parent =
  `Armature` space, so it is a world displacement), along **+Z**; `pelvis`
  translation stays constant within a track. Tracks that travel:
  `Jog_Fwd_Loop` 5.0 m / 0.93 s, `Sprint_Loop` 5.5 m / 0.67 s, `Roll`
  5.0 m, `Swim_Fwd_Loop` 2.9 m, `Crouch_Fwd_Loop` 1.5 m, `Sword_Attack`
  1.5 m, `Walk_Loop` / `Walk_Formal_Loop` 1.3 m, `Push_Loop` 0.8 m,
  `Death01` 0.65 m; all others stay at the origin. `sword_run.glb` is
  in-place, so locomotion tracks will normally be converted with
  `--in-place` (`retargeting.md` §6); `root` stays unmapped so its motion
  folds into `mixamorig:Hips` when root motion is wanted.
- Scale relation to the destination: height 1.83 m vs ~645 units
  (×352), hips 0.917 m vs 413 units (×450). The proportions differ, so
  the Procrustes `k` will land between the two; phase 9 decides whether
  the hips-height ratio should override it for root translation
  (`--frame-scale`).
- `Swim_Idle_Loop` moves the pelvis to −0.33…−0.43 m (below the
  library's floor) and `Swim_Fwd_Loop` to ≈ −0.07 m: swimming tracks are
  the one legitimate case of hips below the destination's feet.
- 2 materials, no textures/images; mesh has `TEXCOORD_1`. All ignored.
