# Web Tool

A browser front end for the retargeter, built with Emscripten from the
same C sources as the command-line tool plus a thin web layer in
`web-tool/`. It opens a source GLB and a destination GLB from the local
machine, lists the source tracks, previews the destination model in a
raylib viewer and exports the retargeted GLBs. Nothing leaves the
browser: files are copied into the WebAssembly module's in-memory file
system (MEMFS) and the output bytes come back the same way.

```
web-tool/
├── CMakeLists.txt        # emcmake project: core sources + web layer + raylib
├── inc/
│   ├── cgltf_rename.h    # force-included: renames our cgltf symbols (see below)
│   ├── web_session.h     # incremental session over the core modules
│   └── web_viewer.h      # raylib viewer surface
├── src/
│   ├── web_session.c     # pure C, compiled into the native test suite
│   ├── web_api.c         # EMSCRIPTEN_KEEPALIVE exports, one static session
│   ├── web_pose.c        # clip → world-space poses in skin order (tested natively)
│   ├── web_viewer.c      # raylib: model load, camera, bone overlay, playback
│   ├── web_viewer_fit.c  # display-only fit matrix (up axis, scale, grounding)
│   └── web_main.c        # main(): start the viewer loop
└── www/
    ├── index.html        # top bar + two columns
    ├── app.js            # file inputs, track list, options, export, canvas size
    └── style.css
```

## Build and run

```
make init-web      # fetch raylib 6.0 WebAssembly release into external/raylib (once)
make web           # emcmake cmake -S web-tool -B build-web && build
make web-serve     # python3 -m http.server --directory build-web 8080
make web-dist      # copy only the deployable files into build-web/dist/
```

`emcc`/`emcmake` must be on `PATH` (`source <emsdk>/emsdk_env.sh`); the
build was verified with Emscripten 5.0.4. `build-web/` holds
`anim-retarget-web.js` + `.wasm`, the page files and `mappings/` (copied
from `docs/mappings/` after every build). The page must be served over
HTTP (file URLs cannot load the `.wasm` fetch).

**Deploying.** `make web-dist` wipes and refills `build-web/dist/` with
exactly what a static host needs: `index.html`, `app.js`, `style.css`,
`anim-retarget-web.js`, `anim-retarget-web.wasm` and `mappings/*.map`.
Keep that relative layout (the loader fetches the `.wasm` next to its
`.js`, the page fetches `mappings/` relative to itself) and serve
`.wasm` as `application/wasm` (otherwise the browser falls back to a
slower non-streaming instantiation). No special headers are needed; the
build uses no threads.

## Layout

- **Top bar**: "Open source animations" and "Open destination skeleton"
  file buttons, the chosen file names, and a status line (the last
  message the C side printed to `stderr`, red for errors).
- **Left column**: the source track list with a checkbox per track, the
  track duration, a select-all box; below it the bone map (preset
  select, "Load .map" button, editable textarea), the options (in place,
  frame alignment, rest-direction correction, output fps) and the solved
  frame-alignment line; at the bottom **Export selected** and **Export
  all**.
- **Right column**: the raylib canvas, and a bar with Play/Pause, the
  frame slider and label, the bone-overlay toggle, "Reset camera" and
  the mouse hints. Clicking a track *name* previews it (the checkbox
  only selects for export); the previewed row is highlighted.

Any change to the map text or an option re-runs the configuration
(debounced); the export buttons enable only when both files are loaded
and the map resolves.

## Data flow

```
<input type=file> ──FileReader──▶ FS.writeFile('/source.glb')
                                    │ web_load_source(path)
                                    ▼
                         WebSession (web_session.c)
     GltfDoc + Skeleton per side, BoneMap from the textarea text,
     Retarget rebuilt whenever a side, the map or an option changes
                                    │ web_build_glb(i)
                                    ▼
                 TrackConvert_Clip → GlbWriter_Build → ByteBuffer
                                    │ HEAPU8.slice(web_glb_data(), size)
                                    ▼
                  Blob download, or File System Access directory
```

`TrackConvert_Clip` (`src/track_convert.c`) is the exact sampling and
retargeting path the CLI's `convert` uses, and `GlbWriter_Build` is the
CLI's writer, so a file exported from the browser has the same JSON
chunk as `anim-retarget convert` with the same map and options and a
binary chunk equal up to float rounding (cglm runs SSE code natively and
plain C in WebAssembly, so sums are accumulated in a different order). File names follow
`ConvertCommand_OutputPath` (characters outside `[A-Za-z0-9._-]` become
`_`). "Export all" with more than one track asks for a directory in
browsers with the File System Access API (Chromium) and falls back to
one download per file elsewhere.

## Exported C API (`web_api.c`)

| Export | Meaning |
|--------|---------|
| `web_load_source(path)`, `web_load_destination(path)` | load a MEMFS GLB; returns `ErrorCode` |
| `web_configure(map_text, frame_align, rest_align, in_place, fps)` | store map + options, rebuild; `fps <= 0` infers |
| `web_is_ready()` | both rigs loaded and the map resolved |
| `web_animation_count()`, `web_animation_name(i)`, `web_animation_duration(i)` | track list |
| `web_animation_file_name(i)` | malloc'd CLI-style file name (free with `web_free`) |
| `web_alignment_text()` | the `frame: rotate … scale … rms …` line |
| `web_build_glb(i)` → size, `web_glb_data()`, `web_glb_release()` | export bytes |
| `web_resize(w, h)`, `web_set_show_bones(b)`, `web_reset_camera()` | viewer |
| `web_preview(i)` → frames, `web_refresh_preview()` | retarget track `i` and load it into the viewer; refresh re-runs the current preview after a configure/load |
| `web_set_playing(b)`, `web_is_playing()`, `web_set_frame(f)`, `web_frame()`, `web_frame_count()` | playback |

Strings cross with `ccall(..., ['string'])` and `UTF8ToString`; bytes
with `HEAPU8.slice`, copied before the next call because the heap can
move on growth (`ALLOW_MEMORY_GROWTH`).

## Viewer

- raylib loads the destination GLB itself (`LoadModel` from MEMFS, mesh,
  skin and embedded textures). Its skeleton bind pose is world space,
  the same convention as the tool's `Pose_Global`.
- **Playback.** A preview runs `TrackConvert_Clip` for the track, then
  `WebPoseSet_FromClip` turns the clip's local keys back into world-space
  transforms per frame, ordered by **skin index** (raylib's bone order),
  10 floats per joint (T, R, S). The viewer copies them into a raylib
  `ModelAnimation` and calls `UpdateModelAnimation(model, anim, frame)`
  with a fractional frame every draw, so raylib interpolates between
  keys and CPU-skins the mesh; the bone overlay reads
  `model.currentPose`. The playhead advances by `GetFrameTime()` and
  wraps at the clip length; scrubbing the slider pauses and seeks.
  Changing the map or an option re-runs the preview. Clearing the
  preview resets the mesh to its bind pose.
- **Display-only fit.** The destination file is shown as-is in data; for
  display the viewer rotates the model so the skeleton's up axis points
  to raylib's +Y, scales it to 2 units tall and puts the lowest joint on
  the grid. The up axis is the direction from the root bone to its child
  with the most descendants (hips → spine), snapped to the dominant file
  axis: −Z for `test_player.glb`, +Y for the UE rig. This never touches
  the exported data (`glb-subset.md` §5: the destination is not Y-up and
  must stay that way).
- **Camera** (built-in raylib modes, only while the mouse is active so
  the view does not drift when the pointer merely crosses the canvas):
  left drag → `CAMERA_THIRD_PERSON` (orbit around the target), middle
  drag → `CAMERA_FREE` (pan), right drag → `UpdateCameraPro` pan scaled
  by the mouse delta, wheel → zoom. "Reset camera" restores the default.
- **Bone overlay**: lines parent → joint and a sphere per joint, drawn
  with depth test off so they show through the mesh.
- The canvas follows its column through a `ResizeObserver` →
  `web_resize` → `SetWindowSize`.

## Two Emscripten details worth knowing

- **cgltf twice.** raylib's prebuilt `libraylib.web.a` carries its own
  cgltf implementation (a newer 1.15 snapshot whose `cgltf_data` layout
  differs from the vendored v1.15), so the two cannot share one copy.
  `web-tool/inc/cgltf_rename.h` is force-included (`-include`) into
  every web TU and `#define`s each public `cgltf_*` function to
  `anim_cgltf_*`. Regenerate it after a cgltf upgrade from the native
  object: `nm build/CMakeFiles/anim-retarget.dir/src/gltf_doc.c.o |
  awk '$2=="T" && $3 ~ /^cgltf_/'`.
- **Keyboard capture.** Emscripten's GLFW shim listens for key events
  on `window` in the capture phase and calls `preventDefault` on
  Backspace and Tab, which would break typing in the map textarea.
  `app.js` registers its own capturing listeners *before* the module is
  created and stops propagation for events targeted at inputs.

## Limits

- A destination without a mesh (e.g. `sword_run.glb`) converts fine but
  shows nothing in the viewer (a warning is printed).
- `--src-up`, `--frame-rotate` and `--frame-scale` are not exposed; the
  CLI remains the place for those overrides.
