# GLB / glTF 2.0 Subset

What the tool reads from a GLB, what it writes, and the hard facts about
the three fixtures in `test/data/`. Spec reference:
https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html . Everything
not listed here is **ignored on read and never written**.

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
- Read: reject bad magic, version ≠ 2, chunk overflowing `length`, first
  chunk not JSON, unknown chunk types after the first two (ignore).
- Write: exactly the two chunks above; `length` computed last.

## 2. JSON properties read

| Object | Properties used | Notes |
|--------|-----------------|-------|
| `asset` | `version`, `generator` | `version` must be `"2.0"`; `generator` shown by `info` |
| `scene`, `scenes[].nodes` | root node list | used to find the armature root |
| `nodes[]` | `name`, `children`, `translation`, `rotation`, `scale`, `matrix`, `skin`, `mesh` | `matrix` → decomposed to TRS (error if the 3×3 has shear, i.e. columns not orthogonal within 1e-4); `mesh`/`skin` only used to recognise mesh nodes to skip |
| `skins[]` | `name`, `joints`, `inverseBindMatrices`, `skeleton` | IBMs decoded to 16 floats each (MAT4, column-major) |
| `animations[]` | `name`, `channels[].sampler`, `channels[].target.node`, `channels[].target.path`, `samplers[].input`, `samplers[].output`, `samplers[].interpolation` | paths `translation`/`rotation`/`scale`; `weights` channels ignored with a note |
| `accessors[]` | `bufferView`, `byteOffset`, `componentType`, `count`, `type`, `normalized`, `min`, `max` | `sparse` → unsupported error |
| `bufferViews[]` | `buffer`, `byteOffset`, `byteLength`, `byteStride` | `buffer` must be 0 |
| `buffers[]` | `byteLength`, `uri` | `uri` present → unsupported error (only the embedded BIN chunk) |

Defaults per spec: `translation` `[0,0,0]`, `rotation` `[0,0,0,1]`,
`scale` `[1,1,1]`, `byteOffset` 0, `interpolation` `"LINEAR"`.

Component types to decode: `5126` float (always for translation/scale,
inputs, IBMs); rotation outputs may also be normalized `5120` byte,
`5121` ubyte, `5122` short, `5123` ushort (decode per spec:
`max(c / 127.0, -1.0)`, `c / 255.0`, `max(c / 32767.0, -1.0)`,
`c / 65535.0`). Accessor element types: `SCALAR`, `VEC3`, `VEC4`, `MAT4`.
`byteStride` honoured when present.

CUBICSPLINE samplers store 3 output elements per key (in-tangent, value,
out-tangent); see `retargeting.md` §2 for evaluation.

Quaternions are stored `(x, y, z, w)`. Matrices are column-major. The
node's local matrix is `T · R · S`.

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
- **Every animation input accessor carries `min` and `max`** (required by
  the spec). Output accessors need none.
- Time grid starts at `0` and advances by `1/fps` (see `retargeting.md`
  §2). Note: Blender writes the first key at `1/30 s` (frame 1), which the
  engine tolerates; starting at 0 is standard and must be verified
  in-engine once (`PLAN.md` phase 11).
- No `meshes`, `materials`, `textures`, `images`, `samplers`,
  `extensionsUsed`.
- Floats printed with `%.9g` (round-trip safe for float32); `-0` is fine;
  `NaN`/`Inf` must never reach the writer (assert upstream).
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

### `UAL1_Standard_RM.glb` — ⚠ currently NOT the animation library (144,852 bytes)
Measured content: the same 27 nodes and Mixamo joint names as
`test_player.glb`, the same mesh/materials/textures, and a single 2-key
`"mixamo.com"` track (0.042 s). It is a near-duplicate of the destination
model and contains **no library animations and no foreign skeleton**.

Expected content (to be confirmed by running `info` on the real export):
the Universal Animation Library "Standard" pack with root motion ("RM"):
dozens of named tracks on a UE5-Mannequin-style skeleton (`root`,
`pelvis`, `spine_01…05`, `neck_01`, `head`, `clavicle_l`, `upperarm_l`,
`lowerarm_l`, `hand_l`, `thigh_l`, `calf_l`, `foot_l`, `ball_l` and the
`_r` mirrors), A-pose rest, root motion carried by the `root` bone, Y-up
after glTF export, metre-scale units.

**Action for the user:** replace `test/data/UAL1_Standard_RM.glb` with the
real export before `PLAN.md` phase 11. Until then every phase is testable
with the other two fixtures plus synthetic transforms (see
`testing-guidelines.md`).
