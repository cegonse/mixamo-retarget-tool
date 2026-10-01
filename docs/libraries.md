# Libraries

Three vendored libraries do the generic work; the tool only writes what is
specific to retargeting. All three live under `third_party/` as **git
submodules pinned to release tags** and are initialised by
`scripts/init.sh` (`git submodule update --init`). System-wide packaged
libraries (zlib, cairo, …) are acceptable too when a need arises — find
them with `pkg-config`/`find_package` and record them here — but none is
needed today; the only system library linked is `libm`.

| Library | Tag | Path | License | Role |
|---------|-----|------|---------|------|
| **cgltf** | `v1.15` | `third_party/cgltf/cgltf.h` | MIT | read GLB/glTF: container, JSON, accessors, node transforms |
| **cglm** | `v0.9.6` | `third_party/cglm/include/` | MIT | vectors, quaternions, matrices (header-only inline API) |
| **json-c** | `json-c-0.19-20260627` | `third_party/json-c/` | MIT | build and serialise the output glTF JSON |

## cgltf (read side)

Single header; `#define CGLTF_IMPLEMENTATION` in **exactly one** `.c` file
(`src/gltf_doc.c`), everywhere else plain `#include <cgltf.h>`.

Calls used:
- `cgltf_parse_file(&options, path, &data)` → `cgltf_load_buffers(&options,
  data, path)` → `cgltf_validate(data)`; `cgltf_free(data)` in the
  destructor. Every non-`cgltf_result_success` maps to `ERR_OPEN_INPUT`
  (file errors) or `ERR_BAD_GLB` (everything else), with the result code
  named in the message.
- `cgltf_data` fields: `asset`, `scene`/`scenes`, `nodes` (`name`,
  `parent`, `children`, `has_translation/rotation/scale/matrix` + values,
  `mesh`, `skin`), `skins` (`joints`, `inverse_bind_matrices`, `skeleton`),
  `animations` (`channels[].target_node/target_path/sampler`,
  `samplers[].input/output/interpolation`).
- Accessor decoding: `cgltf_accessor_read_float(accessor, index, out,
  element_count)` and `cgltf_accessor_unpack_floats(accessor, out, count)`.
  Normalised integer rotations, strides and sparse accessors are handled
  by cgltf — do not re-decode.
- `cgltf_node_transform_local(node, m)` / `cgltf_node_transform_world(node,
  m)` give column-major `float[16]`, matching cglm's `mat4` and the glTF
  spec; the tool's own hierarchy walk (`skeleton`) is still used for
  *animated* globals, since cgltf only knows the rest pose.
- Index helpers (`cgltf_node_index`, `cgltf_accessor_index`, …) when a
  pointer must be reported as a number in `info` output.

`cgltf_write.h` is present in the submodule but **not used**: the output
document is assembled with json-c so its shape mirrors `sword_run.glb`
exactly and no `cgltf_data` pointer graph has to be wired by hand.

## cglm (math)

Header-only inline API: `#include <cglm/cglm.h>`, include path
`third_party/cglm/include`. Nothing to build or link.

Conventions that match glTF, so no conversion is ever needed:
- `versor` is `float[4]` ordered **x, y, z, w** — identical to glTF's
  rotation layout.
- `mat4` is **column-major** — identical to glTF accessors and to
  `cgltf_node_transform_*` output.
- Arrays are passed as pointers and results go to a trailing `dest`
  parameter; the tool's own `Transform` helpers follow the same style.

Functions the specs rely on (`retargeting.md`):

| Need | cglm |
|------|------|
| quaternion product, inverse, normalise | `glm_quat_mul`, `glm_quat_inv`, `glm_quat_normalize` |
| rotate a vector by a quaternion | `glm_quat_rotatev` |
| slerp along the shortest path | `glm_quat_slerp` (use `glm_quat_slerp_longest` only to prove the difference in a test) |
| minimal-arc rotation from one direction to another | `glm_quat_from_vecs` (test the anti-parallel case; wrap if it is not handled) |
| axis/angle ↔ quaternion | `glm_quatv`, `glm_quat_axis`, `glm_quat_angle` |
| quaternion ↔ matrix | `glm_quat_mat4`, `glm_mat4_quat` |
| TRS compose / decompose | `glm_translate`, `glm_quat_rotate`, `glm_scale`; `glm_decompose(m, t, r, s)` then `glm_mat4_quat` on `r` for `matrix` nodes |
| Euler angles for the log line | `glm_euler_angles` (radians, XYZ) |
| vectors | `glm_vec3_add/sub/scale/dot/cross/norm/normalize`, `glm_mat4_mulv3` |

Not in cglm, written in-house: the 4×4 symmetric **Jacobi eigen solver**
used by the Horn/Umeyama frame alignment, cubic-spline keyframe
evaluation, and the shear check on decomposed `matrix` nodes.

## json-c (write side)

Built from the submodule with `add_subdirectory(third_party/json-c)` after
setting `BUILD_SHARED_LIBS OFF`, `BUILD_STATIC_LIBS ON`, `BUILD_APPS OFF`,
`BUILD_TESTING OFF`, `DISABLE_WERROR ON` (its own warnings must not break
our `-Werror` build; set them as cache variables before the
`add_subdirectory`). Link the `json-c` target; its interface include
directories provide the headers, so the include form is `#include
<json.h>` (the build-tree form; a system install would be
`<json-c/json.h>` — not used). `json.h` pulls in `linkhash.h`, whose
unused `static` helpers fail `-Werror` when included from C++ tests, so
the top-level CMakeLists marks json-c's interface includes (and cgltf/cglm)
as `SYSTEM` include directories.

Calls used (`glb_writer`):
- Build: `json_object_new_object`, `json_object_object_add`,
  `json_object_new_array`, `json_object_array_add`,
  `json_object_new_string`, `json_object_new_int`,
  `json_object_new_boolean`.
- Floats: `json_object_new_double_s(value, text)` with `text` from
  `snprintf("%.9g")` — guarantees float32 round-trip, locale-independent
  output and no `nan`/`inf` (assert finite before formatting).
  Alternatively `json_c_set_serialization_double_format("%.9g",
  JSON_C_OPTION_GLOBAL)` once at startup; pick one in phase 7 and test it.
- Serialise: `json_object_to_json_string_length(root,
  JSON_C_TO_STRING_PLAIN, &length)` (compact, no spaces); the returned
  buffer is owned by `root`.
- Free: a single `json_object_put(root)` releases the tree (ownership of
  children transfers on `_add`; never `put` a child after adding it).

json-c is not used for reading: cgltf parses the input itself.

## Don't reimplement

| Job | Use |
|-----|-----|
| GLB container + glTF JSON parsing, accessor decoding, rest-pose node transforms | cgltf |
| Quaternion/vector/matrix algebra, slerp, TRS decompose | cglm |
| JSON tree building, string escaping, serialisation | json-c |
| GLB framing on **write** (12-byte header + JSON chunk + BIN chunk, padding) | in-house, ~30 lines (`glb_writer`) |
| Horn/Umeyama solve, Jacobi 4×4 eigen, keyframe sampling, retargeting | in-house |

## Submodule workflow

```
git submodule update --init          # done by scripts/init.sh
git -C third_party/cglm checkout v0.9.7 && git add third_party/cglm   # upgrade = new tag + commit
```
Never edit files inside `third_party/`; wrap or report upstream.
