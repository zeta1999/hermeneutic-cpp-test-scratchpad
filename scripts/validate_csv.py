#!/usr/bin/env python3
import csv
import sys
from collections import defaultdict
from decimal import Decimal
from pathlib import Path

ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('output')

issues = defaultdict(list)

# Check BBO
bbo_path = ROOT / 'bbo' / 'bbo_quotes.csv'
if bbo_path.exists():
    with bbo_path.open() as fh:
        reader = csv.DictReader(fh)
        for idx, row in enumerate(reader, start=1):
            bid = Decimal(row['best_bid_price'])
            ask = Decimal(row['best_ask_price'])
            ts = row['timestamp_ns']
            if ask == 0:
                issues['bbo'].append(f"{ts}: ask price is zero (row {idx})")
            elif bid > ask:
                issues['bbo'].append(f"{ts}: bid {bid} exceeds ask {ask} (row {idx})")
else:
    issues['bbo'].append('file missing')

# Check price bands
price_path = ROOT / 'price_bands' / 'price_bands.csv'
if price_path.exists():
    with price_path.open() as fh:
        reader = csv.DictReader(fh)
        for idx, row in enumerate(reader, start=1):
            bid = Decimal(row['bid_price'])
            ask = Decimal(row['ask_price'])
            ts = row['timestamp_ns']
            if ask != 0 and bid > ask:
                issues['price_bands'].append(f"{ts}: band {row['offset_bps']} bps has bid {bid} > ask {ask} (row {idx})")
else:
    issues['price_bands'].append('file missing')

# Check volume bands monotonicity
volume_path = ROOT / 'volume_bands' / 'volume_bands.csv'
if volume_path.exists():
    with volume_path.open() as fh:
        reader = csv.DictReader(fh)
        current_ts = None
        prev_bid = None
        prev_ask = None
        for idx, row in enumerate(reader, start=1):
            ts = row['timestamp_ns']
            bid = Decimal(row['bid_price'])
            ask = Decimal(row['ask_price'])
            if ts != current_ts:
                current_ts = ts
                prev_bid = None
                prev_ask = None
            if prev_bid is not None and bid > prev_bid:
                issues['volume_bands'].append(f"{ts}: bid price increases with threshold (row {idx})")
            if prev_ask is not None and ask != 0 and prev_ask != 0 and ask < prev_ask:
                issues['volume_bands'].append(f"{ts}: ask price decreases with threshold (row {idx})")
            if ask == 0:
                issues['volume_bands'].append(f"{ts}: ask price is zero for threshold {row['notional']} (row {idx})")
            prev_bid = bid
            prev_ask = ask
else:
    issues['volume_bands'].append('file missing')

if not any(issues.values()):
    print('All CSV sanity checks passed.')
    sys.exit(0)

for key, msgs in issues.items():
    if not msgs:
        continue
    print(f"[{key}] {len(msgs)} issue(s):")
    for msg in msgs[:20]:
        print('  -', msg)
    if len(msgs) > 20:
        print(f"  ... {len(msgs) - 20} more")

sys.exit(1)
