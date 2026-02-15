# ccache

`ccache` caches compiler outputs so rebuilds are much faster. The CMake project
auto-detects `ccache` on configure and wires it up as the compiler launcher when
available. Install it on your host to benefit from that cache:

## macOS

```bash
brew install ccache
```

## Debian/Ubuntu

```bash
sudo apt-get update
sudo apt-get install -y ccache
```

## Fedora/RHEL

```bash
sudo dnf install ccache
```

Verify the tool is present with `ccache --version`, then configure the project
(`cmake --preset relwithdebinfo` etc.). The configure step prints whether
`ccache` was found. Dockerfiles and the devcontainer image already install
`ccache`, and the Dockerfiles mount `/root/.cache/ccache` so Docker builds can
take advantage of it automatically when BuildKit is enabled.

Dependency sources fetched via CMake’s `FetchContent` API live under
`.deps-cache/<platform>/` by default (ignored by Git; `<platform>` matches the
lowercase host `uname -s`). Export `HERMENEUTIC_DEPS_DIR` if you want to share a
different cache directory between worktrees or containers. The cache never
resides under `build*/`, so purging build trees with `rm -rf build*` is safe and
keeps the vendored downloads intact. Even helper artifacts such as the TSAN
script’s host `protoc` binary are staged inside
`.deps-cache/<platform>/host-protoc` for the same reason.
