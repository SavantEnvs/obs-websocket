# syntax=docker/dockerfile:1.7
#
# Commit-image Dockerfile (small form) on top of the org C/C++ base image.
#
#   DOCKER_BUILDKIT=1 docker build -f mayhem/Dockerfile --build-arg COMMIT=$(git rev-parse HEAD) -t mytag .

FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

# obs-websocket is an OBS Studio plugin. It needs libobs and obs-frontend-api from OBS >= 32 (the
# canvas API); Debian trixie ships 30.2, and OBS 33 needs FFmpeg 8 where trixie has 7.1.
USER root
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends \
        cmake ninja-build pkg-config extra-cmake-modules uthash-dev libsimde-dev \
        libjansson-dev zlib1g-dev uuid-dev libglib2.0-dev libcurl4-openssl-dev \
        libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev \
        libx11-dev libx11-xcb-dev libxcb1-dev libxcb-xinput-dev libpulse-dev libwayland-dev \
        libxkbcommon-dev libgl-dev libegl-dev libgbm-dev libopengl-dev libxcb-composite0-dev libxcb-randr0-dev \
        libxcb-shm0-dev libxcb-xfixes0-dev libxcb-xinerama0-dev libxcomposite-dev libxfixes-dev libxinerama-dev \
        libudev-dev libspeexdsp-dev libvulkan-dev libpci-dev libdrm-dev libasound2-dev \
        qt6-base-dev libasio-dev libwebsocketpp-dev nlohmann-json3-dev \
 && rm -rf /var/lib/apt/lists/*

# libobs + obs-frontend-api, built once from the pinned release into a static prefix (SPEC §6.2
# item 19). libobs has no static build mode, so the CMake object files are archived by hand.
ARG OBS_VER=32.2.2
ARG OBS_SHA256=35d3cd0979d65664fada7119fdb612eca7c34b61a1623a330caec74bf72626c4
RUN d=/opt/toolchains/deps/obs-studio-${OBS_VER} && mkdir -p "$d/src" /tmp/dep-build \
 && curl -fsSL -o /tmp/dep.tgz https://github.com/obsproject/obs-studio/archive/refs/tags/${OBS_VER}.tar.gz \
 && echo "${OBS_SHA256}  /tmp/dep.tgz" | sha256sum -c - \
 && tar -xzf /tmp/dep.tgz --strip-components=1 -C "$d/src" && rm -rf "$d/src/.git" && cd /tmp/dep-build \
 && cmake -G Ninja -S "$d/src" -B . -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_C_FLAGS="-g -gdwarf-3" -DCMAKE_CXX_FLAGS="-g -gdwarf-3" \
        -DCMAKE_INSTALL_PREFIX="$d" -DOBS_VERSION_OVERRIDE=${OBS_VER} \
        -DENABLE_FRONTEND=OFF -DENABLE_BROWSER=OFF -DENABLE_PLUGINS=OFF -DENABLE_SCRIPTING=OFF \
        -DENABLE_NEW_MPEGTS_OUTPUT=OFF -DENABLE_RELOCATABLE=OFF \
 && cmake --build . --target libobs obs-frontend-api -j"$(nproc)" \
 && cmake --install . --prefix "$d" --component Development \
 && llvm-ar rcs "$d/lib/libobs.a" $(find libobs/CMakeFiles -name '*.o' ! -name obs-nix-x11.c.o) \
 && llvm-ar rcs "$d/lib/libobs-frontend-api.a" $(find frontend/api -name '*.o') \
 && cp deps/libcaption/libcaption.a "$d/lib/" \
 && rm -rf "$d"/lib/*.so* "$d/lib/cmake" "$d/lib/pkgconfig" \
 && cd / && rm -rf /tmp/dep-build /tmp/dep.tgz && chmod -R a+rX "$d"
USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh — repo ships no functional tests"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
