
- all builds to use ccache by default if available else default to installed compiler (g++/clang++)
- add doc to install of ccache in docs
- build docker/.devcontainer ~> add ccache to the mix 
- download caches (FetchContent) should land in .deps-cache/ (gitignored) with optional HERMENEUTIC_DEPS_DIR override; docker builds must mount the same path.
- never place dependency caches under build*/ so devs can `rm -rf build*` without re-fetching vendored deps.
