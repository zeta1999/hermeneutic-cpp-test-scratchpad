#!/usr/bin/env python3
import argparse
import json
import math
import random
from pathlib import Path


def clamp_price(price: float, base_price: float, band: float) -> float:
    floor = max(0.1, base_price * (1.0 - band))
    ceil = base_price * (1.0 + band)
    return max(floor, min(ceil, price))


def make_snapshot(mid_price: float, base_price: float, depth: int, quantity: float, rng: random.Random) -> dict:
    bids = []
    asks = []
    for level in range(depth):
        spread_pct = 0.0005 * (level + 1)
        bid_price = clamp_price(mid_price * (1.0 - spread_pct) - rng.uniform(0.0, 5.0), base_price, 0.2)
        ask_price = clamp_price(mid_price * (1.0 + spread_pct) + rng.uniform(0.0, 5.0), base_price, 0.2)
        level_quantity = quantity * (1.0 + 0.2 * level)
        bids.append({
            "price": f"{bid_price:.2f}",
            "quantity": f"{level_quantity:.2f}",
        })
        asks.append({
            "price": f"{ask_price:.2f}",
            "quantity": f"{level_quantity * 0.8:.2f}",
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
                    quantity: float,
                    drift: float,
                    volatility: float) -> None:
    rng = random.Random(seed)
    seq = 1
    active_orders = []
    mid_price = base_price
    price_band = 0.15

    with output.open("w", encoding="utf-8") as handle:
        handle.write(json.dumps(make_snapshot(mid_price, base_price, depth, quantity, rng)) + "\n")
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
                dt = 1.0 / 60.0
                normal = rng.gauss(0.0, 1.0)
                mid_price = mid_price * math.exp((drift - 0.5 * volatility ** 2) * dt + volatility * math.sqrt(dt) * normal)
                mid_price = clamp_price(mid_price, base_price, price_band)
                level = rng.randint(0, max(0, depth - 1))
                spread_pct = 0.0005 * (level + rng.random())
                if side == "bid":
                    raw_price = mid_price * (1.0 - spread_pct) - rng.uniform(0.0, 3.0)
                else:
                    raw_price = mid_price * (1.0 + spread_pct) + rng.uniform(0.0, 3.0)
                price = clamp_price(raw_price, base_price, price_band)
                scale = (0.5 + rng.random()) * (1.0 + 0.15 * level)
                level_quantity = quantity * scale
                payload = {
                    "type": "new_order",
                    "sequence": seq,
                    "order_id": order_id,
                    "side": side,
                    "price": f"{max(0.1, price):.2f}",
                    "quantity": f"{level_quantity:.2f}",
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
    parser.add_argument("--depth", type=int, default=8, help="Depth of snapshot bids/asks.")
    parser.add_argument("--quantity", type=float, default=2500.0, help="Base quantity for snapshot levels.")
    parser.add_argument("--drift", type=float, default=0.0, help="Drift term for the synthetic GBM mid-price.")
    parser.add_argument("--volatility", type=float, default=0.01, help="Volatility term for the synthetic GBM mid-price.")
    args = parser.parse_args()

    args.output.parent.mkdir(parents=True, exist_ok=True)
    generate_events(args.exchange,
                    args.output,
                    args.events,
                    args.seed,
                    args.base_price,
                    args.depth,
                    args.quantity,
                    args.drift,
                    args.volatility)


if __name__ == "__main__":
    main()
