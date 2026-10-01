# Retargeting Specification

The core of the tool: how a source animation on skeleton **S** becomes an
animation on destination skeleton **D** when bone names, rest poses,
world orientation and scale all differ. Read `glb-subset.md` first for
the data model and the fixture facts.

## 0. Notation

- Joint `j` has parent `p(j)`, local rest transform
  `L_j = T(t_j) · R(r_j) · S(s_j)` and global rest `G_j = G_p(j) · L_j`
  (the armature root's own transform included; the scene has none).
- `rot(M)` is the rotation part of a transform as a unit quaternion;
  `pos(M)` its translation. Quaternion products compose right-to-left:
  `a · b` applies `b` first.
- Animated quantities carry `(t)`: `L^a_j(t)`, `G^a_j(t)`.
- The **bone map** `m` is a set of pairs `(s → d)`, source joint to
  destination joint, injective on both sides. Unmapped source joints still
  contribute to their descendants' global transforms; unmapped
  destination joints stay at rest (locally).
- A **root pair** is a mapped pair whose destination joint has no mapped
  ancestor (normally exactly one: hips/pelvis).

Only rotations are retargeted for non-root joints. Root pairs also get
translation. Scale is never retargeted; destination rest scale is emitted.

## 1. Pipeline

```
source GLB ──► skeleton S + track ──► sample pose at t_i (§2)
destination GLB ──► skeleton D
bone map ──► pairs
                       │
            frame alignment Q, k (§3)  ──┐
            rest correction A_s (§4)   ──┤
                       ▼                  │
      per frame: global deltas → D globals → D locals (§5, §6)
                       ▼
            keyframes → output GLB (glb-subset.md §4)
```

Everything is computed once per track; the two alignment steps depend
only on the rest poses and the map, so they are computed once per run and
logged.

## 2. Sampling the source track

Channels have independent key times and interpolation modes, so the pose
is evaluated on a **uniform grid** and re-keyed:

- `fps`: `--fps` if given, else inferred from the track as
  `round(1 / median(Δt))` over the input accessor with the most keys
  (fallback 30 if a sampler has fewer than 2 keys).
- `t_start = min(input)`, `t_end = max(input)` across the track's samplers.
  Frame count `N = round((t_end − t_start) · fps) + 1`, times
  `t_i = t_start + i / fps`, written out as `i / fps` (grid starts at 0).
- Per channel evaluation at time `t` (clamped to `[first, last]` key):
  - `STEP`: value of the last key `≤ t`.
  - `LINEAR`: translation/scale lerp; rotation **slerp along the shortest
    path** (negate `b` if `dot(a, b) < 0`) and normalise.
  - `CUBICSPLINE`: per spec, with `τ = (t − t_k) / (t_{k+1} − t_k)`,
    `p = (2τ³ − 3τ² + 1)·v_k + (τ³ − 2τ² + τ)·Δt·b_k + (−2τ³ + 3τ²)·v_{k+1} + (τ³ − τ²)·Δt·a_{k+1}`
    where `a` are in-tangents and `b` out-tangents; rotations are
    normalised after evaluation.
- A joint with no channel for a path uses its rest value for that path.
- Result: `L^a_j(t_i)` for every source joint, then `G^a_j(t_i)` by the
  hierarchy walk.

## 3. Frame alignment (source world → destination world)

The two files may use different up axes, facings and units
(`test_player.glb` is −Z-up and centimetre-scale; a glTF export of a
UE/Blender rig is +Y-up and may be metre-scale). The tool estimates a
**similarity transform** `(k, Q, c)` — uniform scale, rotation,
translation — that maps source world space onto destination world space:

- Point sets: `P_s = { pos(G_s) }` and `P_d = { pos(G_d) }` over all mapped
  pairs `(s → d)`, in rest pose.
- Solve `argmin Σ ‖ k·Q·p_s + c − p_d ‖²` with the closed form of
  Horn (1987) / Umeyama (1991): centre both sets, build the 3×3
  cross-covariance, form Horn's symmetric 4×4 `N` matrix, take the
  eigenvector of its largest eigenvalue as `Q` (a **Jacobi eigenvalue
  iteration** on a 4×4 symmetric matrix is ~50 lines and all that is
  needed), then `k = Σ (Q·p̃_s)·p̃_d / Σ ‖p̃_s‖²` (ratio of the rotated
  source spread onto the destination spread; always positive), and `c`
  from the centroids. `Q` is proper (det +1) by construction.
- Need ≥ 3 non-collinear pairs; otherwise `Q = I`, `k = 1` and a warning.
- Only `Q` and `k` are used afterwards (`c` is implied by anchoring the
  root pair's rest position, §6).
- `info`-style log line, always printed by `convert`:
  `frame: rotate (X°, Y°, Z°)  scale k  rms residual r`.
  A large residual (> 10 % of the destination's joint spread) means the
  map is wrong or the rest poses are not comparable; print a warning.
- Overrides: `--frame-rotate X,Y,Z` (degrees, applied Z·Y·X) and
  `--frame-scale k` replace the solved values; `--no-frame-align` sets
  `Q = I, k = 1`.

Rest poses need not be the same pose (A vs T) for this to work: the
torso and legs dominate the least-squares fit, and §4 cleans up the limbs.

## 4. Rest-pose correction (A-pose vs T-pose)

With `Q` applied, a source joint in rest may still point in a different
direction than its destination counterpart (UE Mannequin rests in A-pose,
Mixamo in T-pose). For each mapped pair `(s → d)` define the **bone
direction** in destination world space:

- `dir_s = normalize( Q · (pos(G_child(s)) − pos(G_s)) )`
- `dir_d = normalize( pos(G_child(d)) − pos(G_d) )`

where `child(s)` is the *mapped child* of `s` (the mapped descendant whose
destination is a child of `d` in the mapped-only hierarchy). Then:

- If `s` has **exactly one** mapped child: `A_s = Quat_FromTo(dir_s, dir_d)`,
  the minimal-arc rotation taking the source direction onto the destination
  direction.
- Otherwise (a leaf such as a hand, head or toe, or a fork such as hips or
  the upper spine): `A_s = A_parent`, inherited from the nearest mapped
  ancestor; `A = I` at the root pair.

`A_s` is a world-space rotation (destination frame). Twist about the bone
axis is **not** corrected (minimal arc has no twist information); this is
a known limitation and the reason `--no-rest-align` exists. A future
per-bone manual offset (`--bone-offset d=X,Y,Z`) is an extension point,
not part of the plan.

## 5. Rotation retargeting

For each frame `t_i` and mapped pair `(s → d)`:

1. Source world delta: `Δ_s(t) = rot(G^a_s(t)) · rot(G_s)⁻¹`
   (how the bone rotated in source world space relative to its rest).
2. Move it into destination world space: `Δ'_s(t) = Q · Δ_s(t) · Q⁻¹`.
3. Destination global rotation:
   `rot(G^a_d(t)) = Δ'_s(t) · A_s⁻¹ · rot(G_d)`.

Sanity checks implied by the formula (and tested):
- Source at rest (`Δ = I`): `rot(G^a_d) = A_s⁻¹ · rot(G_d)` — the
  destination adopts the *source's* rest pose (A-pose if the source is
  A-posed). ✔ a source animation that starts at rest looks like the
  source.
- Source moves from its A rest to T (`Δ' ≈ A_s`): `rot(G^a_d) ≈ rot(G_d)`
  — the destination lands on its own T rest. ✔
- Identical skeletons, identity map, no alignment: output locals equal
  source locals exactly (ε float noise).

Unmapped destination joints: `G^a_d(t) = G^a_p(d)(t) · L_d` (rest local).

## 6. Root translation

For each root pair `(s → d)`:

- `p_s(t) = pos(G^a_s(t))` — includes the motion of any unmapped source
  ancestors (e.g. a UE `root` bone carrying root motion), so root motion is
  folded into the destination hips automatically.
- `p_d(t) = pos(G_d) + k · Q · (p_s(t) − pos(G_s))`.
- `--in-place`: let `u = normalize(Q · up_s)` be the destination up vector,
  with `up_s = (0, 1, 0)` by default (glTF is +Y-up; `--src-up` overrides
  with `X|Y|Z|-X|-Y|-Z`). Replace the displacement `δ = p_d(t) − pos(G_d)`
  by its vertical part `(δ · u) u`, i.e. drop the horizontal travel.
  Hips bob is kept; the character runs on the spot. Note that for
  `test_player.glb` `u ≈ (0, 0, −1)`.

Non-root joints keep destination rest translation.

## 7. Back to local space and keyframes

Walk destination joints parents-first:

- `rot(L^a_d(t)) = rot(G^a_p(d)(t))⁻¹ · rot(G^a_d(t))`
- root pairs: `pos(L^a_d(t)) = G^a_p(d)(t)⁻¹ · p_d(t)` (parent is normally
  the identity `Armature` node); others: `pos(L^a_d) = t_d`.
- scale: `s_d`.

Rotation keys are **sign-continuous**: if `dot(q_i, q_{i−1}) < 0` negate
`q_i` so LINEAR interpolation in the engine never takes the long way round.
Keys are written as `float32`.

## 8. Diagnostics printed by `convert`

For every track: name, frames, fps, duration; the frame alignment line
(§3); the number of mapped / unmapped source joints and unmapped
destination joints (listing names with `--verbose`); the output path. A
non-finite value anywhere in the keyframes is an internal error (exit
code for malformed output, never a silently written file).

## 9. Validation ladder

1. **Identity**: `test_player → test_player`, identity map → bit-for-bit
   equal rotations.
2. **Near-identity**: `sword_run → test_player`, identity map → equal
   within a few degrees (rest poses differ slightly) and the hips path
   within 1 unit.
3. **Synthetic frame change**: `sword_run` rotated 90° and scaled ×0.01 in
   memory → result equals step 2 within ε. Proves §3 and §6.
4. **Synthetic A-pose**: `sword_run` with its arm chain rest rotated 45°
   down (and the animation adjusted so the world motion is unchanged)
   → with rest alignment the result equals step 2 within ε on the arm
   joints. Proves §4.
5. **Real library**: the UAL export → in-engine check next to
   `sword_run.glb` (manual, `PLAN.md` phase 11).
