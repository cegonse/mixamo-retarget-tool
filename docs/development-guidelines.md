# Development Guidelines

Conventions for `anim-retarget`, a single C command-line tool that
retargets skeletal animation from a source GLB to a destination GLB. One
Bash script sits around it (`scripts/init.sh`) to fetch the test
framework. See `glb-subset.md` for what the tool reads and writes and
`retargeting.md` for what it actually computes.

## Scope reminder

The C tool does **one thing**: load the animation tracks and armature of a
source GLB, load the armature of a destination GLB, map the source motion
onto the destination bones, and write **one model-less GLB per animation
track**. It does **not** render, preview, or edit models, and it does
**not** read or write meshes, materials, textures, lights or cameras. Keep
the tool focused; resist pulling asset-pipeline concerns into it.

## Language and toolchain

- **C99** (`-std=c99`). This is the target standard; don't rely on C11/C17
  features.
- **GNU extensions**: avoid. Use one only if there's no reasonable C99 way
  to do the job, and note why at the use site. Portability across the
  developer's *IX systems (Linux now, macOS likely) matters more than
  convenience.
- **libc first**: prefer standard library calls for file I/O, string
  handling, math and diagnostics (`fopen`/`fread`/`fwrite`,
  `fprintf(stderr,…)`, `<string.h>`, `<stdlib.h>`, `<math.h>`). Don't
  reach for platform APIs when libc covers it. The only linked library is
  `libm`.
- **No non-portable calls** unless wrapped and justified. Nothing that
  ties the build to a single OS.
- **No third-party source.** JSON parsing/writing, GLB framing and the
  math are written in-house (they are small; see `glb-subset.md`). Do not
  vendor cgltf, jsmn, cJSON or similar.
- **CMake** for the build (`cmake_minimum_required(VERSION 3.22)`).
  Out-of-source builds. A thin `Makefile` orchestrates `init`/`all`/`test`.

## Coding style

- **Readability over performance.** No clever tricks, no premature
  optimization. Clear, boring code that's easy to follow wins. This is a
  batch file converter, not a hot loop.
- **No comments in source.** Let descriptive names carry the meaning.
  Do not add comments unless *extremely* necessary — reserved for a genuine
  footgun that names alone cannot convey (a format edge case, a sign
  convention). Never narrate the code. Default to zero comments.
- **Descriptive names** for variables and functions. `destination_joint`,
  not `dj`; `read_accessor_floats`, not `raf`.
- **Function signature form.** Every *public* function matches
  `return_type Module_Function(type arg_name, type other_arg)` — the
  module/type name in `PascalCase`, an underscore, then the function in
  `PascalCase`. **Declarations name their parameters**, not just their
  types. This applies to the app entry point too:
  `int App_Run(int argc, char **argv)` (the one function `main()` calls).
- **Static (file-local) functions use `lowerCamelCase`:** `readHeader()`,
  `appendByte()`, `evaluateChannel()`. The `Module_` prefix is reserved for
  the public surface, so the casing tells you at a glance whether a function
  is part of a module's API or a private helper.
- **No domain magic numbers — back them with a named enum.** GLB chunk
  types, glTF component types, accessor element types, animation paths,
  interpolation modes, header offsets and the like get a named `enum`
  value (e.g. `GlbChunkType`, `GltfComponentType`, `AnimationPath`) rather
  than an inline literal. Introduce the enum value when the code first
  needs it; don't pre-populate unused constants.
- **Short functions.** Aim for 15–20 lines. Not a hard rule, but if a
  function grows past that, look for a block to extract.
- **Indentation: 2 spaces, no tabs.** One additional 2-space level per
  nesting depth.
- **Indent continuations by a block, don't align to parentheses.** When a
  call or expression wraps, indent the continuation lines by one extra
  2-space level rather than lining them up under the opening `(`. Keeps
  diffs small and lines short.
- **Opening brace on the same line (K&R).** For function definitions and
  control blocks alike, the `{` sits next to the `)` / keyword, not on its
  own line. Example:
  ```c
  int App_Run(int argc, char **argv) {
    if (argc < 2) {
      fprintf(stderr,
        "usage: %s <info|convert> ...\n",
        argv[0]);
      return ERR_BAD_ARGS;
    }
    return ERR_NONE;
  }
  ```
- **`static inline` freely.** Extract logical blocks into `static inline`
  helpers in the `.c` file to keep the main function readable. This is
  encouraged, not exceptional.
- **Short files.** Aim for ~150 lines per file. Not strict. **Exception:**
  data-only files (large constant tables) are exempt.
- **Includes use global scope.** Favor `#include <foo.h>` over
  `#include "foo.h"`. Configure the build with `-I` include paths
  (`target_include_directories`) so headers resolve globally. This applies
  to the project's own headers too.
- **Floating point is `float`** for everything stored (glTF stores
  `float32`); intermediate math may widen to `double` where it helps the
  Procrustes solve, but keep conversions explicit.

## Module pattern — opaque structs

Encapsulate behavior behind opaque struct pointers. The header exposes the
type name and functions; the definition lives in the `.c` file. This keeps
internals private and gives every module a clear surface.

```c
// glb_file.h
#pragma once
#include <error_code.h>
#include <stddef.h>
#include <stdint.h>

typedef struct GlbFile GlbFile;

GlbFile *GlbFile_Load(const char *path, ErrorCode *error);
void GlbFile_Destroy(GlbFile *self);
const char *GlbFile_JsonText(GlbFile *self);
const uint8_t *GlbFile_BinaryChunk(GlbFile *self);
size_t GlbFile_BinaryLength(GlbFile *self);
```

Conventions this illustrates:
- `#pragma once` in every header.
- **No `extern "C"` in the tool's own headers or `.c` sources** — the tool
  is pure C. The only C++ in the project is the test files; when a test
  `.cpp` includes a C header, it wraps the include in `extern "C"` at the
  include site (see `testing-guidelines.md`). Keep the linkage concern on
  the C++ side, out of the C sources.
- One opaque type per module, named in `PascalCase`.
- Functions namespaced `TypeName_Method`, taking `self` as the first
  parameter (the "class instance" idiom).
- Constructors return a heap pointer (or `NULL` on failure) and take an
  `ErrorCode *` out-parameter for failure detail.
- Every constructor has a matching destructor. See memory rules below.
- Accessors are named for what they return (`JsonText`, `BinaryLength`).

**Exception — small value types.** `Vec3`, `Quat`, `Mat4` and `Transform`
(translation/rotation/scale) are plain structs passed **by value** with
pure functions (`Quat_Multiply(a, b)`, `Vec3_Cross(a, b)`). Opaque pointers
would make the math unreadable. They never allocate.

## Memory management

- **Prefer static/stack over heap.** Where a module can use fixed internal
  storage or stack buffers instead of allocating, do so. Less to leak, less
  to test, simpler lifetimes. (A loaded GLB, a parsed JSON tree and the
  sampled keyframes are heap — that's fine; it's the many small incidental
  allocations to avoid.)
- **When you do use the heap:**
  - Always pair creation with destruction: a `_Create`/`_Load`/`_Parse` has
    a matching `_Destroy`. No orphan allocators.
  - Every heap-using module must be **covered by tests** that exercise the
    create/destroy pair (so leaks show up under ASan/LSan).
  - Destructors accept `NULL` harmlessly (so cleanup paths are simple).
  - Set freed pointers' owners to a clear state; don't leave dangling
    references in structs.
- Free in reverse order of acquisition; on partial-construction failure,
  unwind cleanly (free what was allocated so far, return `NULL`).

## Error handling

- Use a shared `ErrorCode` type (out-parameter) for recoverable failures
  in constructors/parsers. Define the codes centrally (`error_code.h`).
- Report user-facing errors to `stderr` with `fprintf`; keep `stdout` for
  actual program output (`info` listing, conversion summary).
- Fail loudly and early on malformed input — a bad GLB should produce a
  clear diagnostic (what was expected, where), not a crash or silent wrong
  output. Unsupported-but-valid input (external `.bin` buffers, data URIs,
  sparse accessors, non-TRS `matrix` nodes with shear) is also a clear
  error, not a guess.
- Exit codes: `0` success, non-zero on failure. Distinct codes: bad
  arguments, cannot open input, malformed/unsupported GLB, unknown
  animation or bone name, cannot write output (see `cli.md`).

## Project layout

```
.
├── CMakeLists.txt
├── Makefile              # thin wrapper: init / all / test / clean
├── src/                  # tool source (.c)
├── inc/                  # tool headers (.h), included via <...>
├── test/
│   ├── unit/             # Cest unit tests, one per module
│   ├── acceptance/       # whole-app tests driving App_Run on fixtures
│   └── data/             # fixture GLBs (see glb-subset.md)
├── scripts/
│   └── init.sh           # fetch Cest header + runner into external/
├── external/             # created by init.sh; git-ignored
│   └── cest/
└── docs/                 # this documentation set
```

- `external/` is **git-ignored** and populated by `init.sh`. Never vendor
  the downloaded files into the repo.
- The tool's own headers go in `inc/` and are included as `<name.h>` with
  `inc/` on the `-I` path.
- Fixtures live in `test/data/` (not `tests/fixtures/`); CMake passes the
  absolute path as `FIXTURES_DIR`.

## The script: `scripts/init.sh`

Gets the test dependencies ready so the user needs nothing pre-installed
beyond a C/C++ toolchain, CMake, curl and git.

Responsibilities:
1. Create `external/cest/` if absent.
2. Fetch **Cest v5** from the release
   https://github.com/cegonse/cest/releases/tag/v5 : the header asset
   (named `cest`) and the platform-matching `cest-runner` binary (select by
   `uname -s`/`uname -m`; sha256-verified; mark executable; smoke-test with
   `--help`). See `testing-guidelines.md` for the asset table.
3. Verify the artifacts exist afterward and report their paths; exit
   non-zero with a clear message if anything is missing.

Guidelines:
- Idempotent: re-running re-downloads only what is missing or fails its
  checksum.
- `#!/usr/bin/env bash` with `set -euo pipefail`.
- Detect missing prerequisites (cmake, C compiler, curl) up front and give
  an actionable error.

## What not to do

- Don't add mesh/material/texture/camera/light support. Unknown JSON is
  ignored on read and never written.
- Don't "fix" the destination model's coordinate conventions (see
  `glb-subset.md` — the fixture is deliberately not Y-up). The output must
  reproduce the destination armature verbatim.
- Don't optimize the retargeter for speed at the cost of clarity.
- Don't add heap allocation without a destructor and a test.
