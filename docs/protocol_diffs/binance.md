# Binance vs Mock Feed

Reference: [Binance Spot Diff. Depth Stream](https://binance-docs.github.io/apidocs/spot/en/#diff-depth-stream)

| Topic | Binance | Mock (`cex_type1`) |
| --- | --- | --- |
| Transport | WebSocket payloads named `depthUpdate` with `b`/`a` arrays of price-level deltas | WebSocket NDJSON objects with `type` fields (`snapshot`, `new_order`, `cancel_order`) |
| Sequencing | Uses `U`, `u`, `pu` range semantics to detect gaps and requires local snapshots via REST | Simple monotonically increasing `sequence` integer per event; snapshots arrive inline over the stream |
| Orders | Aggregated depth per price, no distinct order ids | Every `new_order` carries a unique numeric `order_id` so cancels remove exact lots |
| Snapshots | Must pull `/depth` REST snapshot then stitch with stream | `snapshot` events arrive on the same connection before deltas |
| Authentication | Optional (public feed) | Mandatory Bearer token header on upgrade request |

Our protocol intentionally collapses Binance's depth semantics to a deterministic per-order model so unit tests can track true FIFO order ids while still enforcing a sequence counter.
