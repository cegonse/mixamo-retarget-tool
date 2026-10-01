#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXTERNAL_DIR="${REPO_ROOT}/external"
RAYLIB_DIR="${EXTERNAL_DIR}/raylib"
RAYLIB_VERSION="6.0"
RAYLIB_ASSET="raylib-${RAYLIB_VERSION}_webassembly"
RAYLIB_URL="https://github.com/raysan5/raylib/releases/download/${RAYLIB_VERSION}/${RAYLIB_ASSET}.zip"
RAYLIB_SHA256="032a5def0ca36e9e172f7a81c70ae710564e096fbc2b929c20e0778a7e2e3926"
RAYLIB_ZIP="${EXTERNAL_DIR}/${RAYLIB_ASSET}.zip"

fail() {
  echo "init-web.sh: $*" >&2
  exit 1
}

require_command() {
  command -v "$1" >/dev/null 2>&1 || fail "missing prerequisite '$1': $2"
}

check_prerequisites() {
  require_command emcc "install the Emscripten SDK and run 'source <emsdk>/emsdk_env.sh'"
  require_command emcmake "the Emscripten SDK ships it next to emcc"
  require_command cmake "install CMake >= 3.22"
  require_command curl "install curl"
  require_command unzip "install unzip"
}

file_sha256() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | cut -d' ' -f1
  else
    shasum -a 256 "$1" | cut -d' ' -f1
  fi
}

zip_is_valid() {
  [[ -f "${RAYLIB_ZIP}" ]] && [[ "$(file_sha256 "${RAYLIB_ZIP}")" == "${RAYLIB_SHA256}" ]]
}

fetch_zip() {
  local actual
  if zip_is_valid; then
    return
  fi
  curl -fsSL -o "${RAYLIB_ZIP}" "${RAYLIB_URL}"
  actual="$(file_sha256 "${RAYLIB_ZIP}")"
  if [[ "${actual}" != "${RAYLIB_SHA256}" ]]; then
    rm -f "${RAYLIB_ZIP}"
    fail "checksum mismatch for ${RAYLIB_ASSET}.zip: expected ${RAYLIB_SHA256}, got ${actual}"
  fi
}

unpack_zip() {
  local staging="${EXTERNAL_DIR}/${RAYLIB_ASSET}"
  rm -rf "${staging}" "${RAYLIB_DIR}"
  unzip -q "${RAYLIB_ZIP}" -d "${EXTERNAL_DIR}"
  mv "${staging}" "${RAYLIB_DIR}"
}

verify_artifacts() {
  local artifact
  for artifact in "${RAYLIB_DIR}/include/raylib.h" "${RAYLIB_DIR}/lib/libraylib.web.a"; do
    [[ -s "${artifact}" ]] || fail "missing artifact ${artifact}"
    echo "init-web.sh: ok ${artifact}"
  done
}

check_prerequisites
mkdir -p "${EXTERNAL_DIR}"
fetch_zip
if [[ ! -s "${RAYLIB_DIR}/lib/libraylib.web.a" ]]; then
  unpack_zip
fi
verify_artifacts
