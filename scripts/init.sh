#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CEST_DIR="${REPO_ROOT}/external/cest"
CEST_BASE_URL="https://github.com/cegonse/cest/releases/download/v5"

fail() {
  echo "init.sh: $*" >&2
  exit 1
}

require_command() {
  command -v "$1" >/dev/null 2>&1 || fail "missing prerequisite '$1': $2"
}

check_prerequisites() {
  require_command git "install git"
  require_command cmake "install CMake >= 3.22"
  require_command curl "install curl"
  if ! command -v cc >/dev/null 2>&1 && ! command -v gcc >/dev/null 2>&1 && ! command -v clang >/dev/null 2>&1; then
    fail "missing prerequisite: a C compiler (cc, gcc or clang)"
  fi
}

init_submodules() {
  git -C "${REPO_ROOT}" submodule update --init
  local library
  for library in cgltf/cgltf.h cglm/include/cglm/cglm.h json-c/CMakeLists.txt; do
    [[ -f "${REPO_ROOT}/third_party/${library}" ]] || fail "submodule checkout is empty: third_party/${library} missing"
  done
}

runner_os() {
  case "$(uname -s)" in
    Linux) echo linux ;;
    Darwin) echo macos ;;
    MINGW*|MSYS*|CYGWIN*) echo windows ;;
    *) fail "unsupported OS '$(uname -s)' for cest-runner" ;;
  esac
}

runner_arch() {
  case "$(uname -m)" in
    x86_64|amd64) echo x64 ;;
    aarch64|arm64) echo aarch64 ;;
    i686|i386) echo x86 ;;
    *) fail "unsupported architecture '$(uname -m)' for cest-runner" ;;
  esac
}

expected_runner_sha256() {
  case "$1" in
    cest-runner-linux-aarch64) echo 260ac0ecf5a6223405a71dcb70cf35916c3fb8ffc1f9e8290cc788d254e23755 ;;
    cest-runner-linux-x64) echo a227da96cfe59e6a29e8ab390cffdb507abe7ff24c8ace3e7ddd6caac38d956b ;;
    cest-runner-linux-x86) echo 6026d144234a756ffdbd37f9000ed937282816a21e8a9fa224e20ef1edd436af ;;
    cest-runner-macos-aarch64) echo 0fca1326fd7382c3186e8719825cf04c7d020e46a57588d8ca80bf8d1cce53c6 ;;
    cest-runner-macos-x64) echo 90b2d1304036788ed01780c9022d11006ae861470345efb087990dc250816c18 ;;
    *) echo "" ;;
  esac
}

file_sha256() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | cut -d' ' -f1
  else
    shasum -a 256 "$1" | cut -d' ' -f1
  fi
}

runner_is_valid() {
  local path="$1" expected="$2"
  [[ -f "${path}" ]] || return 1
  [[ -z "${expected}" ]] && return 0
  [[ "$(file_sha256 "${path}")" == "${expected}" ]]
}

fetch_runner() {
  local os arch asset expected path actual
  os="$(runner_os)"
  arch="$(runner_arch)"
  asset="cest-runner-${os}-${arch}"
  [[ "${os}" == windows ]] && asset="${asset}.exe"
  expected="$(expected_runner_sha256 "${asset}")"
  path="${CEST_DIR}/cest-runner"
  if ! runner_is_valid "${path}" "${expected}"; then
    curl -fsSL -o "${path}" "${CEST_BASE_URL}/${asset}"
    actual="$(file_sha256 "${path}")"
    if [[ -n "${expected}" && "${actual}" != "${expected}" ]]; then
      rm -f "${path}"
      fail "checksum mismatch for ${asset}: expected ${expected}, got ${actual}"
    fi
  fi
  chmod +x "${path}"
  "${path}" --help >/dev/null 2>&1 || fail "cest-runner smoke test failed (${path} --help)"
}

fetch_header() {
  local path="${CEST_DIR}/cest"
  if [[ ! -s "${path}" ]]; then
    curl -fsSL -o "${path}" "${CEST_BASE_URL}/cest"
  fi
}

verify_artifacts() {
  local artifact
  for artifact in "${CEST_DIR}/cest" "${CEST_DIR}/cest-runner"; do
    [[ -s "${artifact}" ]] || fail "missing artifact ${artifact}"
    echo "init.sh: ok ${artifact}"
  done
}

check_prerequisites
init_submodules
mkdir -p "${CEST_DIR}"
fetch_header
fetch_runner
verify_artifacts
