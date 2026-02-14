# VS Code Debugging

You can debug the unit test binaries that live under `build.debug/tests/` (the
same tree populated by `scripts/run_ctest_debug.sh`). The repository ships VS
Code tasks/launch configurations that automate the build step and attach the
appropriate debugger for each platform.

## Prerequisites

- Run `scripts/compile_debug.sh` at least once so `build.debug` exists. The
  helper accepts optional targets, e.g. `scripts/compile_debug.sh
  test_service_entrypoints`.
- Linux (amd64 or arm64): install the
  [ms-vscode.cpptools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
  extension so VS Code can drive `gdb`.
- macOS (arm64/x86_64): install the
  [vadimcn.vscode-lldb](https://marketplace.visualstudio.com/items?itemName=vadimcn.vscode-lldb)
  extension. `lldb` ships with Xcode Command Line Tools, so no extra toolchain
  work is required.

## Tasks

`.vscode/tasks.json` exposes three build tasks:

- `build (Debug)` – builds the entire `build.debug` tree.
- `build test_service_entrypoints (Debug)` – builds just the
  `test_service_entrypoints` binary.
- `build test_aggregator_service_exec (Debug)` – builds just the
  `test_aggregator_service_exec` binary.

You can run these via the **Terminal → Run Build Task…** menu or let the launch
configs trigger them automatically.

## Launch Configurations

Run the debugger from the **Run and Debug** pane and pick the configuration that
matches your OS + test:

- `Debug test_service_entrypoints (Linux)` – uses `gdb` (cpptools) and launches
  `build.debug/tests/test_service_entrypoints`.
- `Debug test_service_entrypoints (macOS)` – uses the CodeLLDB extension to
  launch the same binary.
- `Debug test_aggregator_service_exec (Linux)` and `… (macOS)` – hook into the
  `test_aggregator_service_exec` binary.

Each configuration sets `PROJECT_BINARY_DIR` so doctest continues to find data,
invokes the matching build task, and runs from the workspace root. Stop the
process normally to let coverage/tests flush their outputs.
