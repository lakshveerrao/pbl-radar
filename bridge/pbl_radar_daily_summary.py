#!/usr/bin/env python3
"""Send a daily PBL-Radar occupancy summary email."""

from __future__ import annotations

import argparse
from datetime import date, datetime
from pathlib import Path

from pbl_radar_email_bridge import fmt_duration, send_email
from email.message import EmailMessage


DEFAULT_TO = "alerts.com"
LOG_PATH = Path("pbl_radar_email_bridge.log")


def parse_today_events(log_path: Path) -> list[tuple[str, dict[str, str]]]:
    events: list[tuple[str, dict[str, str]]] = []
    if not log_path.exists():
        return events
    for line in log_path.read_text(errors="replace").splitlines():
        if "[sent]" not in line:
            continue
        if "HUMAN_PRESENT" in line:
            events.append(("HUMAN_PRESENT", {}))
        elif "HUMAN_LEFT" in line:
            fields: dict[str, str] = {}
            if "'duration': '" in line:
                fields["duration"] = line.split("'duration': '", 1)[1].split("'", 1)[0]
            events.append(("HUMAN_LEFT", fields))
    return events


def build_summary(recipient: str, events: list[tuple[str, dict[str, str]]]) -> EmailMessage:
    entries = sum(1 for event, _ in events if event == "HUMAN_PRESENT")
    exits = sum(1 for event, _ in events if event == "HUMAN_LEFT")
    total_seconds = 0
    for event, fields in events:
        if event == "HUMAN_LEFT":
            try:
                total_seconds += int(fields.get("duration", "0"))
            except ValueError:
                pass

    today = date.today().isoformat()
    body = [
        f"PBL-Radar daily summary for {today}",
        "",
        f"Human entered events: {entries}",
        f"Human left events: {exits}",
        f"Total detected occupancy time: {fmt_duration(str(total_seconds))}",
        "",
        "Note: This is camera-free Wi-Fi/BLE RF inference.",
    ]

    msg = EmailMessage()
    msg["To"] = recipient
    msg["From"] = "PBL-Radar"
    msg["Subject"] = f"PBL-Radar daily summary - {today}"
    msg.set_content("\n".join(body))
    return msg


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--to", default=DEFAULT_TO)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    events = parse_today_events(LOG_PATH)
    msg = build_summary(args.to, events)
    send_email(msg, args.dry_run)
    print(f"sent daily summary to {args.to}: {len(events)} events")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
