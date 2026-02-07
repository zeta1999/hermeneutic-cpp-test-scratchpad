# Kraken vs Mock Feed

Reference: [Kraken WebSockets Book Feed](https://docs.kraken.com/websockets/)

| Topic | Kraken | Mock (`cex_type1`) |
| --- | --- | --- |
| Payload shape | Arrays where bids/asks appear under numeric channel IDs with price-level tuples | Plain JSON objects per event with named fields |
| Sequencing | `as` (snapshot) and `bs`/`asks` updates with `c` (checksum) but no monotonically increasing counter | Each message carries a strict `sequence` integer |
| Order depth | Price-level increments; Kraken expects clients to maintain aggregated volume per price | Mock feed models fully identified orders with numeric `order_id`s, enabling deterministic cancels |
| Authentication | Public book is anonymous | Mock server enforces Bearer token headers to exercise auth paths |
| Snapshot source | Snapshot arrives as part of subscription response but encoded as nested arrays | Snapshot is a dedicated JSON object produced by the NDJSON file before deltas |

These simplifications let us test reconnection, sequence handling, and per-order state changes without reproducing Kraken's channel-id multiplexing or checksum logic.
