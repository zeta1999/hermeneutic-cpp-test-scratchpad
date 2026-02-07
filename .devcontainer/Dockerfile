FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG CODEX_VERSION=latest
ARG CODEX_MIME_CJS_VERSION=3.0.0
ENV NODE_OPTIONS=--no-warnings

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        pkg-config \
        gdb \
        clang \
        clang-tidy \
        clang-format \
        lldb \
        lld \
        git \
        python3 \
        python3-pip \
        sqlite3 libsqlite3-dev \
        nodejs \
        npm \
    && npm install -g "codex@${CODEX_VERSION}" \
    && CODEX_ROOT="$(npm root -g)/codex" \
    && npm --prefix "${CODEX_ROOT}" install --save-exact "mime@${CODEX_MIME_CJS_VERSION}" \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
