#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------------------------------------
# serde.sh — build / test / benchmark helper for the Serde CMake project
# Supports macOS, Ubuntu/Debian, Fedora/RHEL/CentOS, Arch Linux
# ---------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
BUILD_TYPE="Release"
JOBS="$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)"

usage() {
  cat <<EOF
Usage: $(basename "$0") [command] [options]

Commands:
  build       Build the Serde library (default)
  test        Build and run the JSONTestSuite
  bench       Build and run the benchmark
  all         Build, test, then benchmark
  clean       Remove the build directory

Options:
  --debug     Build in Debug mode (default: Release)
  --shared    Build as shared library (default: static)
  --jobs N    Parallel build jobs (default: auto-detected)
  --build-dir DIR  Override build directory (default: ./build)
  -h, --help  Show this help
EOF
  exit 0
}

# ---- argument parsing -------------------------------------------------------
COMMAND="build"
SHARED="OFF"
EXTRA_BENCH_FILES=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    build|test|bench|all|clean) COMMAND="$1" ;;
    --debug)     BUILD_TYPE="Debug" ;;
    --shared)    SHARED="ON" ;;
    --jobs)      shift; JOBS="$1" ;;
    --build-dir) shift; BUILD_DIR="$1" ;;
    -h|--help)   usage ;;
    # Any *.json argument is treated as an extra benchmark file
    *.json)      EXTRA_BENCH_FILES+=("$1") ;;
    *) echo "Unknown option: $1"; usage ;;
  esac
  shift
done

# ---- dependency check -------------------------------------------------------
check_deps() {
  local missing=()
  for cmd in cmake git; do
    command -v "$cmd" &>/dev/null || missing+=("$cmd")
  done

  if [[ ${#missing[@]} -gt 0 ]]; then
    echo "Missing dependencies: ${missing[*]}"
    echo ""

    if [[ "$(uname)" == "Darwin" ]]; then
      echo "Install with: brew install ${missing[*]}"
    elif command -v apt-get &>/dev/null; then
      echo "Install with: sudo apt-get install -y ${missing[*]}"
    elif command -v dnf &>/dev/null; then
      echo "Install with: sudo dnf install -y ${missing[*]}"
    elif command -v pacman &>/dev/null; then
      echo "Install with: sudo pacman -S --noconfirm ${missing[*]}"
    fi
    exit 1
  fi
}

# ---- cmake configure --------------------------------------------------------
configure() {
  local extra_flags=("$@")
  mkdir -p "$BUILD_DIR"
  # Always reset CXX_FLAGS and linker flags so a previous PGO run can't
  # leave stale -fprofile-instr-use in the CMake cache.  do_pgo overrides
  # them via extra_flags (CMake's last -D for a variable wins).
  cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DBUILD_SHARED_LIBS="$SHARED" \
    -DCMAKE_CXX_FLAGS="" \
    -DCMAKE_EXE_LINKER_FLAGS="" \
    "${extra_flags[@]+"${extra_flags[@]}"}"
}

# ---- build ------------------------------------------------------------------
do_build() {
  echo "==> Configuring ($BUILD_TYPE, shared=$SHARED)..."
  configure
  echo "==> Building with $JOBS jobs..."
  cmake --build "$BUILD_DIR" --parallel "$JOBS"
  echo "==> Build complete."
}

# ---- test -------------------------------------------------------------------
do_test() {
  echo "==> Configuring with tests ($BUILD_TYPE)..."
  configure -DSERDE_BUILD_TESTS=ON -DHF_BUILD_TESTS=ON
  echo "==> Building with $JOBS jobs..."
  cmake --build "$BUILD_DIR" --parallel "$JOBS"

  local suite_bin="$BUILD_DIR/json_test_suite"
  local test_data="$BUILD_DIR/_deps/jsontestsuite-src/test_parsing"
  local hf_bin="$BUILD_DIR/hf_test_suite"
  local serde_bin="$BUILD_DIR/serde_test_suite"

  if [[ ! -f "$suite_bin" ]]; then
    echo "ERROR: json_test_suite binary not found at $suite_bin"
    exit 1
  fi
  if [[ ! -d "$test_data" ]]; then
    echo "ERROR: JSONTestSuite test_parsing directory not found at $test_data"
    exit 1
  fi
  if [[ ! -f "$hf_bin" ]]; then
    echo "ERROR: hf_test_suite binary not found at $hf_bin"
    exit 1
  fi
  if [[ ! -f "$serde_bin" ]]; then
    echo "ERROR: serde_test_suite binary not found at $serde_bin"
    exit 1
  fi

  echo "==> Running JSONTestSuite..."
  echo ""
  "$suite_bin" "$test_data"

  echo ""
  echo "==> Running HeaderForge test suite..."
  echo ""
  "$hf_bin"

  echo ""
  echo "==> Running Serde test suite..."
  echo ""
  "$serde_bin"
}

# ---- benchmark --------------------------------------------------------------
do_bench() {
  local default_file="$SCRIPT_DIR/tests/5MB.json"

  if [[ ! -f "$default_file" ]]; then
    echo "ERROR: benchmark file not found: $default_file"
    exit 1
  fi

  for f in "${EXTRA_BENCH_FILES[@]+"${EXTRA_BENCH_FILES[@]}"}"; do
    if [[ ! -f "$f" ]]; then
      echo "ERROR: benchmark file not found: $f"
      exit 1
    fi
  done

  echo "==> Configuring with benchmark ($BUILD_TYPE)..."
  configure -DSERDE_BUILD_BENCHMARK=ON
  echo "==> Building with $JOBS jobs..."
  cmake --build "$BUILD_DIR" --target benchmark

  local bench_bin="$BUILD_DIR/benchmark"
  if [[ ! -f "$bench_bin" ]]; then
    echo "ERROR: benchmark binary not found at $bench_bin"
    exit 1
  fi

  echo "==> Running benchmark..."
  echo ""
  "$bench_bin" "$default_file" "${EXTRA_BENCH_FILES[@]+"${EXTRA_BENCH_FILES[@]}"}"
}

# ---- clean ------------------------------------------------------------------
do_clean() {
  if [[ -d "$BUILD_DIR" ]]; then
    echo "==> Removing $BUILD_DIR..."
    rm -rf "$BUILD_DIR"
    echo "==> Done."
  else
    echo "Nothing to clean (build dir not found)."
  fi
}

# ---- main -------------------------------------------------------------------
check_deps

case "$COMMAND" in
  build) do_build ;;
  test)  do_test ;;
  bench) do_bench ;;
  all)   do_build; echo ""; do_test; echo ""; do_bench ;;
  clean) do_clean ;;
esac
