# Hermeneutic C++ Trading Playground

This repository models the assignment brief with a CMake-driven C++ workspace. Three mock Central Exchanges stream BTC/USDT order book deltas over authenticated WebSockets (POCO), the aggregator consumes those feeds, maintains per-exchange books, and exposes an authenticated gRPC stream of consolidated best bid/ask data. Client services (BBO, volume bands, price bands) subscribe to that stream and publish derived metrics. Requirement-to-implementation mappings live in `instructions/traceability.md`.

## Project layout

```
├── cmake/                 # FetchContent helpers and shared targets
├── proto/                 # gRPC protocol definition + generated library
├── src/
│   ├── common/            # Decimal type, events, utilities
│   ├── lob/               # Per-exchange limit order book
│   ├── aggregator/        # Aggregation engine + config loader
│   ├── cex_type1/         # WebSocket feed client helpers
│   ├── bbo|volume|price/  # Client computation libraries
├── services/              # Aggregator + client binaries + mock CEX WebSocket server
├── config/                # Aggregator config files
├── data/                  # Sample NDJSON feeds used by mock CEX servers
├── docker/                # Dockerfiles and compose topology
└── tests/                 # doctest-based unit tests
```

## Build & test locally

```bash
cmake --preset relwithdebinfo
cmake --build --preset relwithdebinfo-8jobs       # verbose build
# or: cmake --build --preset relwithdebinfo-8jobs-quiet   # hides "[xx%] Built target" noise
ctest --test-dir build
```

The build preset pins `CMAKE_BUILD_PARALLEL_LEVEL` to 8 so future `cmake --build`
invocations avoid oversubscribing local CPUs; feel free to opt back into manual
`--parallel` flags if you need a different level of concurrency.

Third-party dependency tests (for example re2's `charclass_test` and similar)
are disabled by default via `RE2_BUILD_TESTING=OFF` so `cmake --build` only
produces the libraries our binaries need. If you really need to exercise those
vendored test suites, reconfigure with `-DRE2_BUILD_TESTING=ON`.

Project tests are mandatory for normal builds: even if a stale `CMakeCache.txt`
flipped `BUILD_TESTING=OFF`, the top-level `CMakeLists.txt` forces it back on so
`ctest --test-dir build-linux` (or `build`) always finds configuration data. If
you truly need to skip tests (for example when packaging), pass both
`-DHERMENEUTIC_ALLOW_TESTLESS_BUILDS=ON` and `-DBUILD_TESTING=OFF` when running
`cmake -S . -B <builddir>`.

### Platform/architecture notes

- macOS/arm64 (Apple silicon) presets target the native toolchain. Create a Linux cross-build tree when needed:

  ```bash
  cmake -S . -B build-linux -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_OSX_ARCHITECTURES=arm64
  cmake --build build-linux
  ```

- Linux/amd64 or Linux/arm64 hosts can stick to the default presets. If you build for both architectures, configure separate directories (for example `build-amd64` and `build-arm64`) and run `cmake --build <dir>` for each.
- Keep host-native and cross-build directories separate so you can flip targets without reconfiguring.
- When building locally on macOS or non-Debian distros, prefer out-of-tree directories that sit **outside** the repo root (for example `cmake -S . -B ../build-macos && cmake --build ../build-macos`). Docker images copy the workspace into `/src`; if you drop a `build/` directory inside the repo, its cached `CMakeCache.txt` will conflict with the container build step. Keeping host builds elsewhere (and the new `.dockerignore`) avoids multi-gigabyte contexts and cache collisions.

Executables land at `build/services/<name>/<name>`. Run three mock exchanges, then the aggregator, then the gRPC clients:

```bash
./build/services/cex_type1_service/cex_type1_service notbinance data/notbinance.ndjson 9001 notbinance-token 150 &
./build/services/cex_type1_service/cex_type1_service notcoinbase data/notcoinbase.ndjson 9002 notcoinbase-token 200 &
./build/services/cex_type1_service/cex_type1_service notkraken data/notkraken.ndjson 9003 notkraken-token 220 &
./build/services/aggregator_service/aggregator_service config/aggregator.json &
./build/services/bbo_service/bbo_service 127.0.0.1:50051 agg-local-token BTCUSDT &
./build/services/volume_bands_service/volume_bands_service 127.0.0.1:50051 agg-local-token BTCUSDT &
./build/services/price_bands_service/price_bands_service 127.0.0.1:50051 agg-local-token BTCUSDT &
```

`aggregator_service` now waits for feed hostnames in the config to resolve before dialing their WebSockets, so the Docker image no longer needs a shim entrypoint. Set `HERMENEUTIC_WAIT_FOR_FEEDS=0` to skip that wait loop during local experiments.
The default local config binds gRPC to `127.0.0.1` (not `0.0.0.0`) so macOS’ sandbox doesn’t block the listener; adjust `config/aggregator.json` if you explicitly need a wildcard bind.

Prefer a one-liner? `scripts/run_local_stack.sh` builds the same binaries and launches them for you, mirroring the compose topology but without Docker. Override knobs via environment variables:

```
# Reuse an existing build directory and adjust concurrency/output paths.
BUILD_DIR=build-release BUILD_PARALLEL=12 OUTPUT_DIR=./output \
  scripts/run_local_stack.sh
```

The script watches all services and tears them down when one of them exits or you press `Ctrl-C`.

Need a quick smoke test that auto‑stops? `scripts/run_local_stack_once.sh` wraps the same workflow but kills the stack after `RUN_DURATION` seconds (default `15`) and immediately runs the CSV validator:

```
RUN_DURATION=20 STACK_LOG=/tmp/local_stack.log \
  scripts/run_local_stack_once.sh
```

It propagates the same `BUILD_DIR`/`OUTPUT_DIR` overrides you pass to `run_local_stack.sh`, so you can reuse artifacts while still getting a single command that builds, runs, and checks the CSVs.

Prefer to exercise the Docker compose demo instead? `scripts/run_docker_stack_once.sh` accepts the same arguments you would pass to `scripts/docker_run.sh` (defaults to `up`), runs it for `RUN_DURATION` seconds, tears the stack down via `docker compose down`, then reuses `scripts/validate_csv.py`:

```
RUN_DURATION=20 STACK_LOG=/tmp/compose.log \
  scripts/run_docker_stack_once.sh --build up
```

That gives you a single “build + run + CSV check” command regardless of whether you want native binaries or the containerized stack.

- WebSocket clients attach a `Bearer <token>` header, and the mock CEX server validates it before streaming NDJSON lines.
- The aggregator gRPC server validates the same style of `Authorization` metadata before emitting server-streamed `AggregatedBook` updates.
- `cex_type1_service` accepts optional `[start_sequence]` (after `[interval_ms]`) to skip replaying any NDJSON
  events whose `sequence` value is below that threshold, useful when you want long-lived servers to fast-forward
  near the end of a capture file.

## Docker + demo stack

The Docker workflow is now split into explicit build and run steps so you can reuse images (and even build multi-arch artifacts with Buildx) before launching the compose demo.

### Build images (host-arch by default)

```
# Reuse the CMake helper target (wraps the individual docker build commands).
cmake --build build --target docker-images

# Or let compose rebuild everything with your overrides.
docker compose -f docker/compose.yml build

# Need a multi-arch manifest? Invoke buildx bake directly.
docker buildx bake \
  --file docker/compose.yml \
  --set *.platform=linux/amd64,linux/arm64 \
  --push
```

All of these paths consume the Dockerfiles under `docker/`. Stick to host-architecture builds (Apple silicon hosts emit arm64 images; Linux/Windows x86 hosts emit amd64) for local testing, and only reach for the `buildx bake` variant when you truly need to publish multi-arch images.


Inspect image sizes with:

```
docker images hermeneutic/* --format "{{.Repository}}:{{.Tag}}\t{{.Size}}"
```

### Run the demo stack

```
# Rebuild images (if needed) then bring up the stack via compose.
# Equivalent to: docker compose -f docker/compose.yml build && docker compose -f docker/compose.yml up
scripts/docker_run.sh --build

# Or reuse existing images and pass any compose args you need (e.g. detached mode).
scripts/docker_run.sh up -d
```

The helper script creates `output/{bbo,volume_bands,price_bands}` under the repo root and maps those directories into the gRPC client containers. Each service writes its CSV stream directly to the host:

- `output/bbo/bbo_quotes.csv`
- `output/volume_bands/volume_bands.csv`
- `output/price_bands/price_bands.csv`

The aggregator container inherits `HERMENEUTIC_WAIT_FOR_FEEDS` from your shell (defaults to `1`), so set `HERMENEUTIC_WAIT_FOR_FEEDS=0 scripts/docker_run.sh up` if you want it to skip hostname waits inside Compose.

The compose file (`docker/compose.yml`) still launches the same services as before (three mock CEX feeds, the aggregator, and the three gRPC clients) and binds the aggregator gRPC endpoint to `localhost:50051`.

## Demo data & rotating fixtures

The `data/*.ndjson` feeds are generated via `scripts/generate_demo_data.py`, which emits deterministic snapshots plus hundreds of order/cancel events per exchange. Regenerate or extend the fixtures whenever you need heavier loads:

```
scripts/generate_demo_data.py --exchange notbinance --output data/notbinance.ndjson \
  --events 1000 --seed 42 --base-price 30000
scripts/generate_demo_data.py --exchange notcoinbase --output data/notcoinbase.ndjson \
  --events 1000 --seed 43 --base-price 30125
scripts/generate_demo_data.py --exchange notkraken --output data/notkraken.ndjson \
  --events 1000 --seed 44 --base-price 29980
```

Key options:

- `--quantity`: Base BTC per snapshot level (defaults to 2500 so the 1M–50M thresholds all fire). Lower this if you want leaner books (for example `--quantity 200`).
- `--depth`: Snapshot depth per side (defaults to 8). Reduce for smaller books or raise for deeper ones.
- `--drift` / `--volatility`: Parameters for the synthetic GBM mid-price; the generator clamps prices within ±20% of `--base-price` so asks never collapse below bids.

Because `scripts/docker_run.sh` binds the `output/` directory into the client containers, you always have a local copy of the CSV output without running `docker cp`. Delete the files between runs if you want a clean capture.

The generator now uses ~2500 BTC per level across eight depth levels (with random jitter and a simple GBM mid-price), so cumulative notionals reach the 1M–50M thresholds the volume-band client emits.

After any demo run, lint the generated CSVs with:

```
scripts/validate_csv.py output
```

The script flags rows where bids exceed asks, asks collapse to zero, or volume-band prices fail to get worse as thresholds increase.

## stdout publishers & verification

Each client variation subscribes to the aggregator via `BookStreamClient`, formats a human-readable line, logs it via `spdlog::info` (stdout), and appends the same data to a CSV file. Formatting helpers live under:

- `hermeneutic::bbo::BboPublisher::format`
- `hermeneutic::volume_bands::formatQuote`
- `hermeneutic::price_bands::formatQuote`

Unit tests in `tests/services/test_publishers.cpp` assert the exact strings emitted, so the stdout contract stays stable.

## Mock CEX protocol

The mock exchange services stream newline-delimited JSON events over WebSocket. Each event carries a
`sequence` and one of the following `type` values:

- `snapshot`: Initializes the book with `bids` and `asks` arrays (each element has `price` and `quantity`).
- `new_order`: Adds a fresh order identified by `order_id`, including `side`, `price`, and `quantity`.
- `cancel_order`: Removes a previously announced `order_id`.

The client feed (`cex_type1::makeWebSocketFeed`) converts those protocol messages into order-book events
that drive the aggregator's `LimitOrderBook`, so snapshot resets and per-order cancels behave like a real
matching engine feed without materializing the entire NDJSON file in memory.

See `docs/protocol_diffs/` for notes comparing this mock format to the real-time feeds published by Binance,
Coinbase, and Kraken.

## Key design notes

- **Precision** is handled via a fixed-point `Decimal` scaled by 10^18 (which fits comfortably in 60 bits). Configure the backend with `-DHERMENEUTIC_DECIMAL_BACKEND=int128|double|wide` to flip between the default `__int128` storage, a "crappy" double-backed variant, or the wide-integer implementation that performs 256-bit mul/div before narrowing back to 128 bits.
- **Mock exchange connectivity** uses POCO WebSocket clients/servers with token auth so the aggregator exercises the same threading and reconnection patterns a production feed would require.
- **gRPC transport** lives in `proto/aggregator.proto`, giving the aggregator server a strongly typed contract and letting downstream publishers use a shared helper to turn proto payloads back into domain structs.
- **Subscribers** attach to `AggregationEngine` via callbacks, so adding additional gRPC services or transports later is just another subscription.
- **Testing** still leverages doctest for Decimal arithmetic, order book maintenance, and aggregation selection logic; integration tests can be layered on by tagging long-running gRPC/WebSocket paths.
