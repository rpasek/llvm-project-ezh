# Copyright 2026 Google LLC. Licensed under the Apache License, Version 2.0.
#
# Hermetic build + host-only EZH *compiler* tests -- no board, no macOS, no
# local toolchain required. Any machine with Docker validates the entire
# compiler contribution (MC encodings, ISel, clang builtins, Sema ranges).
#
#   docker build -t ezh-tests -f ezh/Dockerfile ezh
#
# The build SUCCEEDS iff all EZH compiler tests pass (check_ezh.py runs as the
# final layer). It does NOT run the on-silicon suites -- those need the
# EVK-MIMXRT595 board; see HANDOFF.md.
#
# EZH is an experimental target, so it goes in LLVM_EXPERIMENTAL_TARGETS_TO_BUILD;
# the native targets (X86;AArch64, so the tools run on either host arch) go in
# LLVM_TARGETS_TO_BUILD. Drop whichever native target you don't need to trim the
# build.

FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
      git cmake ninja-build clang lld python3 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

ARG REPO=https://github.com/bogdan-petru/llvm-project-ezh.git
ARG BRANCH=ezh-port
WORKDIR /src
RUN git clone --depth 1 --branch ${BRANCH} ${REPO} .

# Richard's EZH CMakeLists forces -Werror; on an arbitrary host compiler the
# generic LLVM headers at this HEAD can emit warnings that are irrelevant to
# EZH. Relax it for the container build only (no source change is committed).
RUN sed -i 's/-Werror //' llvm/lib/Target/EZH/CMakeLists.txt

RUN cmake -S llvm -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DLLVM_ENABLE_ASSERTIONS=ON \
      -DLLVM_ENABLE_PROJECTS="clang;lld" \
      -DLLVM_TARGETS_TO_BUILD="X86;AArch64" \
      -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD="EZH" \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    && ninja -C build clang llc llvm-mc FileCheck

# Final layer: the 12 host-only compiler tests. Fails the build on any failure.
RUN python3 ezh/check_ezh.py build/bin
