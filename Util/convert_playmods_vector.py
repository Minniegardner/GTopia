#!/usr/bin/env python3
import re
import sys


def parse_rows(text: str):
    rows = []
    for line in text.splitlines():
        line = line.strip()
        if not line.startswith("{"):
            continue
        values = re.findall(r'"((?:[^"\\]|\\.)*)"', line)
        if len(values) < 14:
            continue
        rows.append(values[:14])
    return rows


def parse_item_id(raw: str) -> int:
    raw = (raw or "").strip()
    if not raw:
        return 0
    if "_" in raw:
        raw = raw.split("_", 1)[0]
    return int(raw) if raw.isdigit() else 0


def to_rgba_aarrggbb(raw: str):
    raw = (raw or "").strip()
    if not raw or not raw.isdigit():
        return None
    value = int(raw)
    if value < 0 or value > 0xFFFFFFFF:
        return None
    a = (value >> 24) & 0xFF
    r = (value >> 16) & 0xFF
    g = (value >> 8) & 0xFF
    b = value & 0xFF
    return f"{r},{g},{b},{a}"


def convert(rows):
    out = []
    out.append("# Generated from legacy info_about_playmods vector")
    out.append("# Supported mapping: add_playmod, set_timer, set_skin_color, set_items")
    out.append("# Unsupported legacy fields are ignored by current loader")
    out.append("")

    for row in rows:
        (
            legacy_id,
            consumable_id,
            duration,
            name,
            add_msg,
            remove_msg,
            display_id,
            _state,
            _sound,
            skin,
            _how_work,
            _eff,
            _say_after,
            _gravity,
        ) = row

        item_id = parse_item_id(consumable_id)
        display_item = parse_item_id(display_id) or item_id
        if display_item <= 0:
            continue

        mod_name = name if name else f"Legacy PlayMod {legacy_id}"
        out.append(f"add_playmod|{display_item}|{mod_name}|{add_msg}|{remove_msg}|")

        if duration.isdigit() and int(duration) > 0:
            out.append(f"set_timer|{int(duration)}|")

        rgba = to_rgba_aarrggbb(skin)
        if rgba and skin != "16777215":
            out.append(f"set_skin_color|{rgba}|")

        if item_id > 0:
            out.append(f"set_items|{item_id}|")

        out.append("")

    return "\n".join(out).rstrip() + "\n"


def main():
    if len(sys.argv) != 3:
        print("Usage: convert_playmods_vector.py <legacy_input.txt> <playmods.txt>")
        return 1

    in_path, out_path = sys.argv[1], sys.argv[2]
    with open(in_path, "r", encoding="utf-8") as f:
        text = f.read()

    rows = parse_rows(text)
    if not rows:
        print("No legacy playmod rows found in input.")
        return 2

    output = convert(rows)
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(output)

    print(f"Converted {len(rows)} rows -> {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
