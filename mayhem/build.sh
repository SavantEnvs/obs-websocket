#!/usr/bin/env bash
#
# mayhem/build.sh — builds the obs-websocket protocol harnesses (fuzz + standalone) and the known-answer
# test binary. Runs offline in the commit image as `mayhem` in /mayhem.
#
# obs-websocket is an OBS Studio plugin: its sources are compiled here directly (the plugin entry point
# src/obs-websocket.cpp and the Qt forms are replaced by mayhem/harness/env.cpp, a headless stand-in)
# and linked against the static libobs + obs-frontend-api that the Dockerfile builds into
# /opt/toolchains/deps/obs-studio-<version>.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

OBS=/opt/toolchains/deps/obs-studio-32.2.2
BUILD=mayhem/build
GEN=$BUILD/gen
OUT=${MAYHEM_OUT:-/mayhem}
export PATH="/usr/lib/qt6/libexec:$PATH"

rm -rf "$BUILD"
mkdir -p "$GEN" "$BUILD/fuzz" "$BUILD/kat" "$OUT"

# plugin-macros.generated.h is configured by upstream's CMake from this template.
VERSION=$(sed -n 's/^set(obs-websocket_VERSION \(.*\))$/\1/p' CMakeLists.txt)
RPC_VERSION=$(sed -n 's/^set(OBS_WEBSOCKET_RPC_VERSION \(.*\))$/\1/p' CMakeLists.txt)
sed -e "s/@obs-websocket_VERSION@/$VERSION/" -e "s/@OBS_WEBSOCKET_RPC_VERSION@/$RPC_VERSION/" \
    src/plugin-macros.h.in > "$GEN/plugin-macros.generated.h"

# The one Qt object upstream code needs (the forms that hold the others are not built).
moc -DASIO_STANDALONE src/websocketserver/WebSocketServer.h -o "$GEN/moc_WebSocketServer.cpp"

QT_MODULES="Qt6Core Qt6Gui Qt6Widgets Qt6Network"
QT_CFLAGS=$(pkg-config --cflags $QT_MODULES)
QT_LIBS=$(pkg-config --libs $QT_MODULES)

INCLUDES="-Isrc -Ilib -I$GEN -I$OBS/src/frontend/api -I$OBS/include/obs -I$OBS/src/libobs -I$OBS/src/deps/libcaption $QT_CFLAGS"
CXXFLAGS_COMMON="-std=c++17 -fPIC -DASIO_STANDALONE $INCLUDES"

SYS_LIBS="-L$OBS/lib -lobs-frontend-api -lobs -lcaption $QT_LIBS \
  -lavcodec -lavformat -lavutil -lswscale -lswresample -ljansson -lz -lX11 -lX11-xcb -lxcb -lxcb-xinput \
  -luuid -lpulse -lgio-2.0 -lgobject-2.0 -lglib-2.0 -lwayland-client -lxkbcommon -ldl -lm -lpthread"

PROJECT_SOURCES=$(find src -name '*.cpp' ! -path 'src/forms/*' ! -path 'src/eventhandler/*' ! -name obs-websocket.cpp | sort)
HARNESS_SOURCES="mayhem/harness/env.cpp mayhem/harness/frontend.cpp mayhem/harness/types.cpp mayhem/harness/wrap_sleep.cc"

# compile <flavor dir> <compiler> <flags...> -- <sources...>: one object per source, in parallel.
compile_all() {
  local dir="$1" compiler="$2"; shift 2
  local flags=()
  while [ "$1" != "--" ]; do flags+=("$1"); shift; done
  shift
  printf '%s\n' "$@" | xargs -P "$MAYHEM_JOBS" -I{} bash -c '
    src="$1"; dir="$2"; compiler="$3"; shift 3
    obj="$dir/$(echo "$src" | tr / _).o"
    "$compiler" "$@" -c "$src" -o "$obj"' _ {} "$dir" "$compiler" "${flags[@]}"
}

# 1) The known-answer test binary: the project's normal flags, no sanitizers, no fuzzer.
# shellcheck disable=SC2086
compile_all "$BUILD/kat" "$CXX" -O1 -g $COVERAGE_FLAGS $CXXFLAGS_COMMON -- \
  $PROJECT_SOURCES "$GEN/moc_WebSocketServer.cpp" $HARNESS_SOURCES mayhem/test/kat.cpp
for c in tick platform; do
  "$CC" -O1 -g $COVERAGE_FLAGS -I"$OBS/include/obs" -I"$OBS/src/libobs" -I"$OBS/src/deps/libcaption" \
    -c "mayhem/harness/$c.c" -o "$BUILD/kat/$c.o"
done
# shellcheck disable=SC2086
"$CXX" $COVERAGE_FLAGS "$BUILD"/kat/*.o -Wl,--wrap=nanosleep $SYS_LIBS -o "$OUT/obsws_kat"

# 2) The fuzz build: the project, harness and libobs glue instrumented with $SANITIZER_FLAGS.
FUZZ_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link"
# shellcheck disable=SC2086
compile_all "$BUILD/fuzz" "$CXX" $FUZZ_FLAGS $CXXFLAGS_COMMON -- \
  $PROJECT_SOURCES "$GEN/moc_WebSocketServer.cpp" $HARNESS_SOURCES mayhem/lsan_off.cc \
  mayhem/harness/fuzz_session_json.cpp mayhem/harness/fuzz_auth_json.cpp mayhem/harness/fuzz_session_msgpack.cpp
# shellcheck disable=SC2086
for c in tick platform; do
  "$CC" $FUZZ_FLAGS -I"$OBS/include/obs" -I"$OBS/src/libobs" -I"$OBS/src/deps/libcaption" \
    -c "mayhem/harness/$c.c" -o "$BUILD/fuzz/$c.o"
done
# shellcheck disable=SC2086
"$CC" $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$BUILD/standalone_main.o"

COMMON_OBJS=$(ls "$BUILD"/fuzz/*.o | grep -v '/mayhem_harness_fuzz_')
for target in fuzz_session_json fuzz_auth_json fuzz_session_msgpack; do
  tobj="$BUILD/fuzz/mayhem_harness_${target}.cpp.o"
  # shellcheck disable=SC2086
  "$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE $COMMON_OBJS "$tobj" -Wl,--wrap=nanosleep $SYS_LIBS \
    -o "$OUT/$target"
  # shellcheck disable=SC2086
  "$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link "$BUILD/standalone_main.o" $COMMON_OBJS "$tobj" -Wl,--wrap=nanosleep $SYS_LIBS \
    -o "$OUT/$target-standalone"
done
