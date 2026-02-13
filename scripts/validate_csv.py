#!/usr/bin/env python3
import csv
import sys
from collections import defaultdict
from decimal import Decimal
from pathlib import Path

csv.field_size_limit(10**6)

ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('output')

issues = defaultdict(list)


def parse_int(row, key):
    value = row.get(key)
    if value in (None, ''):
        return 0
    try:
        return int(value)
    except ValueError:
        return 0

# Check BBO
bbo_path = ROOT / 'bbo' / 'bbo_quotes.csv'
if bbo_path.exists():
    with bbo_path.open() as fh:
        reader = csv.DictReader(fh)
        if not reader.fieldnames or 'best_bid_price' not in reader.fieldnames:
            issues['bbo'].append('missing header or columns')
        else:
            row_count = 0
            for idx, row in enumerate(reader, start=1):
                row_count = idx
                bid = Decimal(row['best_bid_price'])
                ask = Decimal(row['best_ask_price'])
                ts = row['timestamp_ns']
                max_feed = parse_int(row, 'max_feed_timestamp_ns')
                min_feed = parse_int(row, 'min_feed_timestamp_ns')
                publish_ns = parse_int(row, 'publish_timestamp_ns')
                if ask == 0:
                    issues['bbo'].append(f"{ts}: ask price is zero (row {idx})")
                elif bid > ask:
                    issues['bbo'].append(f"{ts}: bid {bid} exceeds ask {ask} (row {idx})")
                if publish_ns and max_feed and publish_ns - max_feed > 5_000_000_000:
                    issues['bbo'].append(f"{ts}: publish lag {(publish_ns - max_feed) / 1e9:.2f}s (row {idx})")
                if max_feed and min_feed and max_feed - min_feed > 5_000_000_000:
                    issues['bbo'].append(f"{ts}: feed spread {(max_feed - min_feed) / 1e9:.2f}s (row {idx})")
            print(f"[bbo] rows: {row_count}")
            if row_count == 0:
                issues['bbo'].append('file has zero data rows')
else:
    issues['bbo'].append('file missing')

# Check price bands
price_path = ROOT / 'price_bands' / 'price_bands.csv'
if price_path.exists():
    with price_path.open() as fh:
        reader = csv.DictReader(fh)
        if not reader.fieldnames or 'bid_price' not in reader.fieldnames:
            issues['price_bands'].append('missing header or columns')
        else:
            row_count = 0
            for idx, row in enumerate(reader, start=1):
                row_count = idx
                bid = Decimal(row['bid_price'])
                ask = Decimal(row['ask_price'])
                ts = row['timestamp_ns']
                if ask != 0 and bid > ask:
                    issues['price_bands'].append(f"{ts}: band {row['offset_bps']} bps has bid {bid} > ask {ask} (row {idx})")
            print(f"[price_bands] rows: {row_count}")
            if row_count == 0:
                issues['price_bands'].append('file has zero data rows')
else:
    issues['price_bands'].append('file missing')

# Check volume bands monotonicity
volume_path = ROOT / 'volume_bands' / 'volume_bands.csv'
if volume_path.exists():
    with volume_path.open() as fh:
        reader = csv.DictReader(fh)
        if not reader.fieldnames or 'bid_price' not in reader.fieldnames:
            issues['volume_bands'].append('missing header or columns')
        else:
            current_ts = None
            prev_bid = None
            prev_ask = None
            row_count = 0
            for idx, row in enumerate(reader, start=1):
                row_count = idx
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
            print(f"[volume_bands] rows: {row_count}")
            if row_count == 0:
                issues['volume_bands'].append('file has zero data rows')
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
