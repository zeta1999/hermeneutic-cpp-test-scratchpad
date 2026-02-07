# Hermeneutic C++ Trading Playground

This repository models the assignment brief with a CMake-driven C++ workspace. Three mock Central Exchanges stream BTC/USDT order book deltas over authenticated WebSockets (POCO), the aggregator consumes those feeds, maintains per-exchange books, and exposes an authenticated gRPC stream of consolidated best bid/ask data. Client services (BBO, volume bands, price bands) subscribe to that stream and publish derived metrics.

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
cmake --build --preset relwithdebinfo-8jobs
ctest --test-dir build
```

The build preset pins `CMAKE_BUILD_PARALLEL_LEVEL` to 8 so future `cmake --build`
invocations avoid oversubscribing local CPUs; feel free to opt back into manual
`--parallel` flags if you need a different level of concurrency.

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

- WebSocket clients attach a `Bearer <token>` header, and the mock CEX server validates it before streaming NDJSON lines.
- The aggregator gRPC server validates the same style of `Authorization` metadata before emitting server-streamed `AggregatedBook` updates.

## Docker + demo stack

Every service ships with a dedicated Dockerfile (`docker/Dockerfile.*`). Build the suite then bring up the compose topology (three CEX containers, aggregator, and the three gRPC clients):

```bash
cmake --build build --target docker-images
cd docker
docker compose up --build
```

Expose the aggregator gRPC endpoint on `localhost:50051` and watch client logs roll through the container output.

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

- **Precision** is handled via a fixed-point 128-bit `Decimal` so multiplying prices and volumes for band calculations is deterministic.
- **Mock exchange connectivity** uses POCO WebSocket clients/servers with token auth so the aggregator exercises the same threading and reconnection patterns a production feed would require.
- **gRPC transport** lives in `proto/aggregator.proto`, giving the aggregator server a strongly typed contract and letting downstream publishers use a shared helper to turn proto payloads back into domain structs.
- **Subscribers** attach to `AggregationEngine` via callbacks, so adding additional gRPC services or transports later is just another subscription.
- **Testing** still leverages doctest for Decimal arithmetic, order book maintenance, and aggregation selection logic; integration tests can be layered on by tagging long-running gRPC/WebSocket paths.
