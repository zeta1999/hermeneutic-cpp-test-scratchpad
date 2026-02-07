# Coinbase vs Mock Feed

Reference: [Coinbase Exchange WebSocket `level2` channel](https://docs.cloud.coinbase.com/exchange/docs/websocket-channels#level2)

| Topic | Coinbase | Mock (`cex_type1`) |
| --- | --- | --- |
| Message types | `snapshot`, `l2update` with arrays of `[side, price, size]` | `snapshot`, `new_order`, `cancel_order` JSON objects |
| Order identity | Depth-only; updates replace aggregated size at prices | Each `new_order` introduces a numeric `order_id`; cancels target exact orders |
| Sequencing | Timestamp + best-effort ordering (no explicit gap detection) | Explicit `sequence` field increments per event |
| Auth | Requires signed REST key if subscribing to private channels; public order book is unauthenticated | Always requires Bearer token; keeps mock infra consistent with other services |
| Snapshots | Delivered once per subscription then incremental deltas | Delivered over the same stream before deltas without REST coordination |

We diverge from Coinbase by emitting explicit `cancel_order` events and guaranteeing numeric ids, which keeps downstream tests deterministic without rebuilding price-level aggregation logic.
