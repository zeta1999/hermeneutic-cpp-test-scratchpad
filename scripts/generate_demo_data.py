#!/usr/bin/env python3
import argparse
import json
import math
import random
from pathlib import Path


def make_snapshot(base_price: float, depth: int, quantity: float) -> dict:
    bids = []
    asks = []
    for level in range(depth):
        bids.append({
            "price": f"{base_price - 1.0 * level - 0.25:.2f}",
            "quantity": f"{quantity + 0.1 * level:.2f}",
        })
        asks.append({
            "price": f"{base_price + 1.0 * level + 0.50:.2f}",
            "quantity": f"{quantity + 0.2 * level:.2f}",
        })
    return {
        "type": "snapshot",
        "sequence": 1,
        "bids": bids,
        "asks": asks,
    }


def generate_events(name: str,
                    output: Path,
                    events: int,
                    seed: int,
                    base_price: float,
                    depth: int,
                    quantity: float) -> None:
    rng = random.Random(seed)
    seq = 1
    active_orders = []

    with output.open("w", encoding="utf-8") as handle:
        handle.write(json.dumps(make_snapshot(base_price, depth, quantity)) + "\n")
        for _ in range(events):
            seq += 1
            if active_orders and rng.random() < 0.25:
                order_id = rng.choice(active_orders)
                payload = {
                    "type": "cancel_order",
                    "sequence": seq,
                    "order_id": order_id,
                }
                active_orders.remove(order_id)
            else:
                order_id = int(seq * 100 + rng.randint(1, 50))
                side = rng.choice(["bid", "ask"])
                price_jitter = rng.uniform(-50, 50)
                price = base_price + (price_jitter if side == "bid" else math.fabs(price_jitter))
                quantity_jitter = rng.uniform(0.1, 1.5)
                payload = {
                    "type": "new_order",
                    "sequence": seq,
                    "order_id": order_id,
                    "side": side,
                    "price": f"{max(0.1, price):.2f}",
                    "quantity": f"{quantity + quantity_jitter:.2f}",
                }
                active_orders.append(order_id)
            handle.write(json.dumps(payload) + "\n")
    print(f"[demo-data] wrote {events + 1} events to {output}")


def main():
    parser = argparse.ArgumentParser(description="Generate NDJSON demo feeds.")
    parser.add_argument("--exchange", required=True, help="Exchange name (purely informational).")
    parser.add_argument("--output", required=True, type=Path, help="Output NDJSON file path.")
    parser.add_argument("--events", type=int, default=250, help="Additional events to append after the snapshot.")
    parser.add_argument("--seed", type=int, default=1, help="Random seed for deterministic output.")
    parser.add_argument("--base-price", type=float, default=30000.0, help="Starting price used for snapshot generation.")
    parser.add_argument("--depth", type=int, default=4, help="Depth of snapshot bids/asks.")
    parser.add_argument("--quantity", type=float, default=1.0, help="Base quantity for snapshot levels.")
    args = parser.parse_args()

    args.output.parent.mkdir(parents=True, exist_ok=True)
    generate_events(args.exchange, args.output, args.events, args.seed, args.base_price, args.depth, args.quantity)


if __name__ == "__main__":
    main()
