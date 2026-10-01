# Testing Guidelines

How the retargeter is tested. Uses **Cest** v5 (https://cestframework.com/)
- a header-only, Jest-style C++ framework, with its companion **cest-runner**
binary to launch and aggregate the test executables. Tests are C++ (`.cpp`)
even though the tool is C99; the C modules are compiled and linked in, and
Cest drives them. See `cest-reference.md` for the full API surface.

Two layers: **unit tests** per module, and **acceptance tests** that link
the whole app and check real GLB in -> GLB out using the fixtures in
`test/data/`.

## How Cest is obtained (init.sh, not vendored)

The Cest header and the `cest-runner` binary are downloaded from the v5
GitHub release by `scripts/init.sh` - not committed to the repo:

- Release: https://github.com/cegonse/cest/releases/tag/v5
- The **header** asset is named `cest` (a single header). Fetch it to
  `external/cest/cest` and put that directory on the test include path, so
  tests do `#include <cest>`.
  URL: `https://github.com/cegonse/cest/releases/download/v5/cest`
- The **runner** is a per-platform prebuilt binary. Asset names follow the
  fixed convention `cest-runner-{os}-{arch}` (Windows adds `.exe`):

  | Host OS | Host arch | Asset name | sha256 |
  |---------|-----------|------------|--------|
  | Linux | aarch64/arm64 | `cest-runner-linux-aarch64` | `260ac0ecf5a6223405a71dcb70cf35916c3fb8ffc1f9e8290cc788d254e23755` |
  | Linux | x86_64/amd64 | `cest-runner-linux-x64` | `a227da96cfe59e6a29e8ab390cffdb507abe7ff24c8ace3e7ddd6caac38d956b` |
  | Linux | x86/i686 | `cest-runner-linux-x86` | `6026d144234a756ffdbd37f9000ed937282816a21e8a9fa224e20ef1edd436af` |
  | macOS | arm64 (Apple Silicon) | `cest-runner-macos-aarch64` | `0fca1326fd7382c3186e8719825cf04c7d020e46a57588d8ca80bf8d1cce53c6` |
  | macOS | x86_64 (Intel) | `cest-runner-macos-x64` | `90b2d1304036788ed01780c9022d11006ae861470345efb087990dc250816c18` |
  | Windows | x64 | `cest-runner-windows-x64.exe` | (see release page) |

  Base URL: `https://github.com/cegonse/cest/releases/download/v5/<asset>`

`init.sh` resolves the runner asset deterministically from `uname -s` /
`uname -m` (no API call needed):
- OS: `Linux` → `linux`, `Darwin` → `macos` (Windows/MinGW → `windows`).
- Arch: `x86_64`/`amd64` → `x64`, `aarch64`/`arm64` → `aarch64`,
  `i686`/`i386` → `x86`.
- Download `cest-runner-{os}-{arch}` (+ `.exe` on Windows) to
  `external/cest/cest-runner`, **verify its sha256** against the table,
  mark it `+x`, and smoke-test (`cest-runner --help`).
- Download the `cest` header to `external/cest/cest`.
- Fail clearly if the host maps to no listed asset (unlisted arch) or a
  checksum mismatches, printing expected vs actual hash.

The developer's current machine (Linux x86_64) resolves to
`cest-runner-linux-x64`.

## Cest essentials (see cest-reference.md for the full API)

- **Structure** - one top-level `describe` per file, `it` cases inside,
  nesting allowed:
  ```cpp
  #include <cest>
  extern "C" {
  #include <transform.h>   // C module under test, via <...>
  }

  describe("Transform", []() {
      it("composes with identity", []() {
          Transform result;
          Transform_Compose(&identity, &child, &result);
          expect(result.rotation[3]).toBe(child.rotation[3], 1e-6f);
      });
  });
  ```
- **Assertions** - `expect(v).toBe(x)`/`toEqual`, `.Not->` to negate,
  `toBeNull()`/`toBeNotNull()`, `toBeInRange(a,b)`, floating-point
  `toBe(x, epsilon)`, and for byte/buffer work: `toEqualBytes(expected)`,
  `toEqualMemory(ptr, len)`.
- **Hooks** - `beforeEach`/`afterEach`, `beforeAll`/`afterAll`, one of each
  per suite, reference-capture `[&]` to reach outer state. Shared state
  must live at **file scope** (`static`), not as a local of the `describe`
  lambda: `describe` returns before the tests run, so `[&]` on its locals
  dangles (the binary then exits 1 with no output).
- **Focus/skip** - `fit`/`xit`, `fdescribe`/`xdescribe`, `todo(...)`.
- **Parametrized** - `withParameter<T>().withValue(...).thenDo([](T x){...})`
  for table-driven cases.
- **Failure model** - a failing assertion throws `AssertionError` and stops
  *that* test; other tests continue. Don't use exceptions for normal
  control flow in the tool.
- **Signals/leaks** - the runner reports crashes (SIGSEGV/SIGFPE/...) as
  failures; with ASan on, leaks are reported via `__SANITIZE_ADDRESS__`.

## File naming and layout

- One unit-test file per module, named **`module-name.test.cpp`** (matching
  the module: `transform.test.cpp`, `gltf-doc.test.cpp`,
  `frame-align.test.cpp`, ...).
- Acceptance tests in their own `*.test.cpp` file(s) under
  `test/acceptance/`, using the fixture GLBs.
- Tree:
  ```
  test/
  ├── unit/
  │   ├── app.test.cpp
  │   ├── transform.test.cpp
  │   ├── gltf-doc.test.cpp
  │   ├── skeleton.test.cpp
  │   ├── anim-track.test.cpp
  │   ├── bone-map.test.cpp
  │   ├── frame-align.test.cpp
  │   ├── retarget.test.cpp
  │   ├── glb-writer.test.cpp
  │   └── args.test.cpp
  ├── acceptance/
  │   ├── info.test.cpp
  │   └── convert.test.cpp
  └── data/                # fixture GLBs (committed; see glb-subset.md)
      ├── test_player.glb
      ├── sword_run.glb
      └── UAL1_Standard_RM.glb
  external/cest/
  ├── cest                 # header (downloaded)
  └── cest-runner          # runner binary (downloaded)
  ```

## Building the tests (CMake: one binary per test file)

Mirror Cest's own CMake pattern: **glob `*.test.cpp` and make one
executable per file** (`test_<name>`), each linking the C modules under
test and including the downloaded `cest` header. `cest-runner` then
discovers and runs all of them.

```cmake
# --- test targets (mirrors cegonse/cest CMakeLists pattern) ---
enable_testing()

set(CEST_DIR ${CMAKE_SOURCE_DIR}/external/cest)   # populated by init.sh

set(SAN_COMPILE -fsanitize=address)
set(SAN_LINK -fsanitize=address)
if(NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
  list(APPEND SAN_LINK -static-libasan)
endif()

# C sources of the tool that tests link against (everything EXCEPT main.c)
file(GLOB TOOL_LIB_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.c)
list(REMOVE_ITEM TOOL_LIB_SOURCES ${CMAKE_SOURCE_DIR}/src/main.c)

file(GLOB_RECURSE TEST_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/test/*.test.cpp)

add_custom_target(build_tests)

foreach(TEST_FILE ${TEST_FILES})
  get_filename_component(FILE_NAME ${TEST_FILE} NAME_WE)
  set(EXEC_NAME test_${FILE_NAME})
  add_executable(${EXEC_NAME} EXCLUDE_FROM_ALL ${TEST_FILE} ${TOOL_LIB_SOURCES})
  target_include_directories(${EXEC_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/inc                      # tool headers, included as <name.h>
    ${CMAKE_SOURCE_DIR}/third_party/cgltf        # <cgltf.h>
    ${CMAKE_SOURCE_DIR}/third_party/cglm/include # <cglm/cglm.h>
    ${CEST_DIR})                                 # the 'cest' header
  target_compile_options(${EXEC_NAME} PRIVATE
    -g -O0 -Wall -Wunused-value -Werror
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++20>
    $<$<COMPILE_LANGUAGE:C>:-std=c99>
    ${SAN_COMPILE})
  target_link_options(${EXEC_NAME} PRIVATE ${SAN_LINK})
  target_link_libraries(${EXEC_NAME} PRIVATE json-c m)   # json-c: add_subdirectory target
  target_compile_definitions(${EXEC_NAME} PRIVATE
    FIXTURES_DIR="${CMAKE_SOURCE_DIR}/test/data")
  add_test(NAME ${EXEC_NAME} COMMAND ${EXEC_NAME})
  add_dependencies(build_tests ${EXEC_NAME})
endforeach()
```

Notes:
- `main.c` is **removed** from the sources tests link (see below) - Cest
  provides the test binary's `main()`, so including the tool's `main` would
  cause two `main`s.
- Test targets are `EXCLUDE_FROM_ALL` so a plain build needs no Cest
  header; `make test` builds the `build_tests` aggregate.
- Building test targets in C++20 while the tool is C99 is fine: the C
  modules compile as C and are linked into the C++ test binary. The tool's
  headers stay pure C (no `extern "C"` in them); the test `.cpp` wraps the C
  includes in `extern "C"` at the include site so they link cleanly:
  ```cpp
  #include <cest>
  extern "C" {
  #include <app_main.h>
  #include <gltf_doc.h>
  }
  ```
- The `json-c` target comes from `add_subdirectory(third_party/json-c)`
  in the top-level CMakeLists (see `libraries.md` for the cache options to
  set first); cgltf and cglm are header-only includes. The cgltf
  implementation is compiled once inside `src/gltf_doc.c`, so test files
  must **not** define `CGLTF_IMPLEMENTATION`.
- Tests that write files use a per-test temporary path under the build
  tree (`FIXTURES_DIR` is read-only input; never write into `test/data/`).

## Running the tests (cest-runner)

`cest-runner` launches every test binary in a directory and aggregates
results. After building:

```
make test                                   # init if needed, build, run all
external/cest/cest-runner build/            # run all test_* in build/
external/cest/cest-runner build/ --grep quat
external/cest/cest-runner build/ --watch    # interactive
```

CTest also works (`ctest` runs each `add_test`), but the project standard
is **cest-runner over the built binaries** for the richer aggregated/tree
output.

## The core entry point (main must be a pass-through)

To let acceptance tests drive the whole app, **`main()` is a thin
pass-through to a callable core function** - no logic in `main`:

```c
// app_main.h
#pragma once
int App_Run(int argc, char **argv);

// main.c   (excluded from the test build)
#include <app_main.h>
int main(int argc, char **argv) { return App_Run(argc, argv); }
```

The acceptance test calls `App_Run(...)` directly with a constructed argv
(`info`/`convert`, fixture paths, flags, output dir), then inspects the
output. `main.c` is excluded from the test build at the CMake level (the
`list(REMOVE_ITEM ... main.c)` above). Keep `main.c` a one-liner so nothing
of value is lost.

`App` writes its listing/summary through an output-stream seam
(`App_SetOutputStream(FILE *stream)`, default `stdout`) so `info` tests can
capture into a `tmpfile()` and assert on the text.

## Unit tests

Test each module's public surface in isolation. The expected coverage per
module (see `PLAN.md` for the phase each belongs to):

- **transform** (thin layer over cglm): compose/inverse round-trip on a
  known TRS; `Transform_FromNode` for TRS nodes and for `matrix` nodes
  (decompose recovers T/R/S; a sheared matrix is rejected);
  `Transform_MinimalArc(a, b)` rotates `a` onto `b` for perpendicular,
  near-parallel and **anti-parallel** inputs (this pins down whether
  `glm_quat_from_vecs` needs a wrapper); `Transform_EulerDegrees` of a
  90° X rotation reads (90, 0, 0); shortest-path slerp midpoint between
  equivalent rotations of opposite sign stays near the endpoints.
- **gltf_doc** (wrapper over cgltf): loads each fixture; node
  count/names/children/TRS from `test_player.glb` (25 joints under
  `mixamorig:Hips`, root `Armature` at node 26); skin joints order and 25
  inverse bind matrices read through the accessor helpers; animation
  channels/samplers with decoded input/output floats (`sword_run`: 21
  keys 0.0333→0.7); a missing file → `ERR_OPEN_INPUT`; a truncated or
  garbage GLB → `ERR_BAD_GLB` naming the `cgltf_result`; destroy frees
  everything (ASan).
- **skeleton**: hierarchy order (parents before children), parent indices,
  global rest transforms (Hips global == local since `Armature` is
  identity; `Spine` global translation == Hips·Spine), name lookup.
- **anim_track**: STEP, LINEAR (slerp for rotations, shortest path) and
  CUBICSPLINE evaluation at, between and outside key times; uniform
  resampling grid (`sword_run`: 21 keys at 30 fps → 0.7 s).
- **bone_map**: parses `a=b,c=d`, map files with comments/blank lines,
  names containing `:`; errors on unknown source or destination names
  listing the offending name; duplicate destination → error.
- **frame_align**: Horn/Umeyama solve recovers a known rotation + uniform
  scale + translation applied to a point cloud (parametrized over several
  rotations incl. 90° about each axis and a 0.01 scale); degenerate input
  (< 3 points, collinear) falls back to identity with a warning flag.
- **retarget**: with identical source and destination skeletons and an
  identity map the output local rotations equal the input (ε 1e-5);
  A-pose → T-pose correction brings the mapped bone direction onto the
  destination's direction; unmapped destination joints keep rest; root
  translation scales by `k` and rotates by `Q`; `--in-place` zeroes the
  horizontal displacement.
- **glb_writer** (json-c + in-house framing): emitted document has
  nodes/skin/animation/accessors/bufferViews/buffers in the documented
  shape; every animation input accessor carries `min`/`max`; byte offsets
  are 4-byte aligned; JSON chunk padded with spaces, BIN with zeros; no
  `mesh`/`material` keys; floats serialised with `%.9g` (a value like
  `0.1f` round-trips exactly); the produced bytes parse back through
  `gltf_doc` (cgltf) and pass `cgltf_validate`.
- **args**: both commands, every flag, missing/duplicate/invalid values →
  `ERR_BAD_ARGS` with a usage message.

**Heap coverage.** Every module that allocates has a test exercising the
**create/destroy pair**, run under ASan so leaks surface. Destructors must
accept `NULL`.

## Acceptance tests (whole-app, always green)

Keep a **working set of acceptance-level tests** at all times: the entire
app is linked and run against the fixture GLBs, verifying input -> output.

- **info**: `App_Run("info", test_player.glb)` output lists the
  `mixamo.com` track, 25 joints and the indented hierarchy with
  `mixamorig:Hips` at the top; same for `sword_run.glb`.
- **convert, identity**: `test_player.glb` → `test_player.glb` with the
  25-pair identity map (built in the test from the skeleton names); the
  output file loads, has 25 joints in the same order, the same IBMs, and
  rotation keys equal to the source within ε.
- **convert, sword_run → test_player**: identity map, default options; the
  output's local rotations match `sword_run.glb` within a loose tolerance
  (the two fixtures' rest poses differ by a couple of degrees at the
  shoulders), hips translation keys match within 1 unit, 21 keys, duration
  0.7 s, scale channels present.
- **convert, synthetic frame change**: the test builds a transformed copy
  of `sword_run.glb` in memory (armature rotated 90° about X and scaled
  ×0.01, animation transformed accordingly), writes it to a temp GLB, then
  retargets it onto `test_player.glb`; the result must match the plain
  `sword_run → test_player` result within ε. This proves frame alignment
  without depending on the UAL fixture.
- **convert, multiple tracks**: on `UAL1_Standard_RM.glb`, `--anim
  Idle_Loop,Walk_Loop` yields two files named after the tracks,
  `--all-anims` yields 43; each loads back and validates. `A_TPose`
  converted onto `test_player.glb` gives rotations within a few degrees
  of the destination rest pose.
- **malformed input**: truncated/garbage GLB fails cleanly (non-zero exit,
  message on stderr, no crash under ASan).
- Output files go to a temp directory under `build/`; tests clean up in
  `afterEach`.

The ultimate acceptance criterion is **"the output plays correctly in the
game engine next to `sword_run.glb`"**; that step is manual and recorded in
`PLAN.md`.

## Stub injection for external calls

Make side-effecting calls testable via a **function-pointer seam**:
production injects the real libc-backed implementation, tests inject a stub
they observe. Pattern:

```c
// file_io.h
#pragma once
#include <stddef.h>
#include <stdint.h>

int FileIo_WriteBytes(const char *path, const uint8_t *data, size_t size);
void FileIo_SetWriteBytesFunction(int (*fn)(const char *, const uint8_t *, size_t));
```

- The module holds a static function pointer defaulting to a real
  implementation (`fopen`/`fwrite`/`fclose`); `FileIo_WriteBytes` calls
  through it.
- The setter swaps it; `NULL` restores the default. Production never calls
  the setter; **tests** install a stub and reset it in `afterEach`.

Seam guidelines:
- Apply to boundaries that are awkward in a test: file writes and the
  output stream. One setter per swappable call; keep the interface tiny.
- The seam is for **boundaries**, not internal logic. Test pure functions
  (math, retarget) directly - no seam.

## Sanitizers

- Test targets build with **ASan/LSan** as in the CMake above; the runner
  reports leaks via `__SANITIZE_ADDRESS__`. This is what makes the
  create/destroy pair tests meaningful.
- The malformed-input tests run under ASan; the runner also catches any
  signal as a failure.

## CI order
`scripts/init.sh` (init submodules, fetch Cest) -> build tool -> build tests -> `cest-runner
build/` (unit + acceptance, ASan on).

## What good coverage looks like here
- Every module: constructor/destructor, happy path, one malformed/edge case.
- GLB: fixtures load through cgltf; produced files parse back and
  validate; malformed input rejected.
- Math: transform helpers over cglm have known-answer cases; Horn solve
  recovers a known similarity transform.
- Retarget: identity is identity; frame change is undone; A→T correction
  verified on bone directions.
- Acceptance: `info` on both fixtures; identity and frame-change converts
  compare against `sword_run.glb`.
