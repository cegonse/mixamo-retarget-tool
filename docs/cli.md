# Command Line Interface

Binary name: `anim-retarget`. Two commands. All diagnostics go to
`stderr`; listings and summaries go to `stdout`.

## `info` — inspect a GLB

```
anim-retarget info <file.glb> [--rest]
```

Prints, in this order:

```
file: test/data/sword_run.glb
asset: glTF 2.0, generator "Khronos glTF Blender I/O v5.1.19"
animations: 1
  [0] "mixamo.com"  0.700 s  21 keys @ 30 fps  75 channels (T 25, R 25, S 25)  LINEAR,STEP
skins: 1
  [0] "Armature"  25 joints  inverseBindMatrices: yes
armature (skin 0, root node 25 "Armature"):
  mixamorig:Hips
    mixamorig:Spine
      mixamorig:Spine1
        ...
```

- The hierarchy is the depth-first joint tree (parents before children,
  children in `children` order), two spaces per depth level.
- `--rest` appends each joint's rest `T=(x, y, z) R=(x, y, z, w) S=(x, y, z)`
  on the same line.
- Works on any of the three fixtures; a file with no skins prints
  `skins: 0` and no armature section; no animations prints
  `animations: 0`.

## `convert` — retarget tracks

```
anim-retarget convert <source.glb> <destination.glb>
    (--anim <name>[,<name>...] | --all-anims)
    (--map <src=dst>[,<src=dst>...] | --map-file <path>) [both allowed; later wins per source bone]
    --out-dir <dir> | --out <file.glb>
    [--fps <n>] [--in-place] [--src-up X|Y|Z|-X|-Y|-Z]
    [--no-frame-align] [--frame-rotate <X,Y,Z>] [--frame-scale <k>]
    [--no-rest-align] [--verbose]
```

Arguments:

| Flag | Meaning |
|------|---------|
| `<source.glb>` | file holding the animation tracks and the source armature |
| `<destination.glb>` | file holding the target armature (`test_player.glb`) |
| `--anim` | comma-separated track names, matched exactly against `animations[].name`; an index (`#3`) is accepted for unnamed tracks |
| `--all-anims` | convert every track |
| `--map` | `srcBone=dstBone` pairs, comma-separated; names may contain `:` (Mixamo) and are matched exactly |
| `--map-file` | same pairs, one per line; `#` starts a comment; blank lines ignored |
| `--out-dir` | directory for `<track>.glb` outputs (created if missing); required unless `--out` |
| `--out` | explicit output file; only valid when exactly one track is converted; its parent directory is created if missing |
| `--fps` | resampling rate; default inferred from the source track (`retargeting.md` §2) |
| `--in-place` | strip horizontal root displacement (`retargeting.md` §6) |
| `--src-up` | source up axis used by `--in-place`; default `Y` |
| `--no-frame-align` / `--frame-rotate` / `--frame-scale` | disable or override the solved frame transform (§3) |
| `--no-rest-align` | disable per-bone rest-direction correction (§4) |
| `--verbose` | list unmapped joints, per-track alignment details |

Summary printed per track (stdout):

```
track "Jog_Fwd_Loop": 29 frames @ 30 fps (0.933 s)
  frame: rotate (-90.0°, 0.0°, 0.0°)  scale 380.00  rms 3.2
  joints: 24 mapped, 41 source unmapped, 1 destination unmapped
  wrote out/Jog_Fwd_Loop.glb (41,208 bytes)
```

Unknown track or bone names are errors that list what *is* available.

## Exit codes

| Code | Name | When |
|------|------|------|
| 0 | `ERR_NONE` | success |
| 1 | `ERR_BAD_ARGS` | usage error (also prints usage) |
| 2 | `ERR_OPEN_INPUT` | cannot read a GLB path |
| 3 | `ERR_BAD_GLB` | malformed or unsupported GLB/glTF content |
| 4 | `ERR_NOT_FOUND` | unknown animation, bone or skin |
| 5 | `ERR_WRITE_OUTPUT` | cannot create the output dir/file or write failed |
| 6 | `ERR_INTERNAL` | non-finite result or invariant violation |

`main()` returns `App_Run(argc, argv)` unchanged.

## Example (Mixamo → Mixamo smoke test)

```
anim-retarget info test/data/sword_run.glb
anim-retarget convert test/data/sword_run.glb test/data/test_player.glb \
  --anim mixamo.com --map-file docs/mappings/mixamo-identity.map --out out/sword_run.glb
```

## Example (UAL → test_player)

```
anim-retarget info test/data/UAL1_Standard_RM.glb        # discover names
anim-retarget convert test/data/UAL1_Standard_RM.glb test/data/test_player.glb \
  --anim Jog_Fwd_Loop,Walk_Loop --map-file docs/mappings/ual-to-mixamo.map --out-dir out/ual \
  --in-place --no-rest-align
anim-retarget convert test/data/UAL1_Standard_RM.glb test/data/test_player.glb \
  --all-anims --map-file docs/mappings/ual-to-mixamo.map --out-dir out/ual --no-rest-align
```

Both rigs rest in T-pose, so `--no-rest-align` is recommended for this
pair (`retargeting.md` §4, phase 9 finding).

`docs/mappings/` holds the maintained map files (identity map for the
Mixamo rig; UAL → Mixamo once the real fixture exists).
