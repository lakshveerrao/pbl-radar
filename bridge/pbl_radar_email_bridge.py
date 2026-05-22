#!/usr/bin/env python3
"""Email bridge for PBL-Radar presence events.

Reads USB serial lines from the T-HMI firmware and sends an email when a human
is detected or leaves. Uses SMTP when configured; otherwise tries macOS Mail.
"""

from __future__ import annotations

import argparse
import os
import smtplib
import subprocess
import sys
import time
from datetime import datetime
from email.message import EmailMessage

import serial


DEFAULT_PORT = "/dev/tty.usbmodem1101"
DEFAULT_TO = "alerts.com"


def parse_line(line: str) -> tuple[str, dict[str, str]] | None:
    if not line.startswith("PBL_RADAR_"):
        return None
    parts = line.strip().split(",")
    if len(parts) < 2:
        return None
    kind = parts[0].replace("PBL_RADAR_", "")
    event = parts[1] if kind == "EVENT" else kind
    fields: dict[str, str] = {}
    for part in parts[2:]:
        if "=" in part:
            key, value = part.split("=", 1)
            fields[key.strip()] = value.strip()
    return event, fields


def fmt_duration(seconds_text: str | None) -> str:
    try:
        seconds = int(seconds_text or "0")
    except ValueError:
        seconds = 0
    mins, secs = divmod(seconds, 60)
    hours, mins = divmod(mins, 60)
    if hours:
        return f"{hours}h {mins}m {secs}s"
    if mins:
        return f"{mins}m {secs}s"
    return f"{secs}s"


def int_field(fields: dict[str, str], key: str, default: int = 0) -> int:
    try:
        return int(fields.get(key, str(default)) or default)
    except ValueError:
        return default


def inferred_present(fields: dict[str, str], previous: bool | None) -> bool:
    explicit = fields.get("present") == "1"
    confidence = int_field(fields, "confidence")
    room = int_field(fields, "room")
    motion = int_field(fields, "motion")
    silhouette = int_field(fields, "silhouette")

    strong = confidence >= 72 and (silhouette >= 45 or motion >= 65 or room >= 85)
    weak = confidence >= 58 and silhouette >= 35 and motion >= 45
    quiet = confidence <= 42 and silhouette <= 32 and motion <= 38

    if explicit or strong or weak:
        return True
    if quiet:
        return False
    return bool(previous)


def build_message(event: str, fields: dict[str, str], recipient: str) -> EmailMessage:
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    if event == "HUMAN_PRESENT":
        subject = "PBL-Radar: Human presence detected"
        body = [
            "Detected: Human",
            f"Time: {now}",
            f"Confidence: {fields.get('confidence', '?')}%",
        ]
    elif event == "HUMAN_LEFT":
        subject = "PBL-Radar: Human left the room"
        body = [
            "Human left the room",
            f"Time: {now}",
            f"Duration: {fmt_duration(fields.get('duration'))}",
            f"Final confidence: {fields.get('confidence', '?')}%",
        ]
    else:
        subject = f"PBL-Radar: {event}"
        body = [f"Event: {event}", f"Time: {now}"]

    body.extend(
        [
            "",
            "Signal details:",
            f"Room score: {fields.get('room', '?')}%",
            f"Motion score: {fields.get('motion', '?')}%",
            f"Silhouette score: {fields.get('silhouette', '?')}%",
            "",
            "Source: PBL-Radar camera-free Wi-Fi/BLE RF inference.",
        ]
    )

    msg = EmailMessage()
    msg["To"] = recipient
    msg["From"] = os.environ.get("PBL_RADAR_FROM", os.environ.get("SMTP_USER", "PBL-Radar"))
    msg["Subject"] = subject
    msg.set_content("\n".join(body))
    return msg


def send_via_smtp(msg: EmailMessage) -> bool:
    host = os.environ.get("SMTP_HOST")
    user = os.environ.get("SMTP_USER")
    password = os.environ.get("SMTP_PASS")
    if not host or not user or not password:
        return False
    port = int(os.environ.get("SMTP_PORT", "587"))
    with smtplib.SMTP(host, port, timeout=20) as smtp:
        smtp.starttls()
        smtp.login(user, password)
        smtp.send_message(msg)
    return True


def osa_escape(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def send_via_macos_mail(msg: EmailMessage) -> None:
    body = msg.get_content()
    script = f'''
tell application "Mail"
  activate
  set newMessage to make new outgoing message with properties {{subject:"{osa_escape(msg["Subject"])}", content:"{osa_escape(body)}", visible:true}}
  tell newMessage
    make new to recipient at end of to recipients with properties {{address:"{osa_escape(msg["To"])}"}}
    send
  end tell
end tell
'''
    subprocess.run(["osascript", "-e", script], check=True)


def send_via_command_mail(msg: EmailMessage) -> bool:
    mail_bin = "/usr/bin/mail"
    if not os.path.exists(mail_bin):
        return False
    subprocess.run(
        [mail_bin, "-s", str(msg["Subject"]), str(msg["To"])],
        input=msg.get_content(),
        text=True,
        check=True,
        timeout=20,
    )
    return True


def send_email(msg: EmailMessage, dry_run: bool) -> None:
    if dry_run:
        print(f"[dry-run] {msg['Subject']} -> {msg['To']}")
        print(msg.get_content())
        return
    if send_via_smtp(msg):
        return
    send_via_macos_mail(msg)


def main() -> int:
    parser = argparse.ArgumentParser(description="Email PBL-Radar human presence events.")
    parser.add_argument("--port", default=DEFAULT_PORT)
    parser.add_argument("--to", default=DEFAULT_TO)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    present: bool | None = None
    present_since = 0.0
    last_sent = 0.0
    last_status_log = 0.0

    def send_transition(event: str, fields: dict[str, str]) -> None:
        nonlocal last_sent
        now = time.time()
        if now - last_sent < 3:
            return
        msg = build_message(event, fields, args.to)
        try:
            send_email(msg, args.dry_run)
            last_sent = now
            print(f"[sent] {event} {fields}")
        except Exception as exc:
            print(f"[error] failed to send {event}: {exc}", file=sys.stderr)

    print(f"PBL-Radar email bridge listening on {args.port}; recipient {args.to}")
    print("SMTP env vars are optional: SMTP_HOST SMTP_PORT SMTP_USER SMTP_PASS")
    while True:
        try:
            with serial.Serial(args.port, 115200, timeout=1) as ser:
                print(f"[serial] connected to {args.port}")
                while True:
                    raw = ser.readline()
                    if not raw:
                        continue
                    line = raw.decode("utf-8", errors="replace").strip()
                    parsed = parse_line(line)
                    if not parsed:
                        continue
                    event, fields = parsed
                    if event == "STATUS":
                        is_present = inferred_present(fields, present)
                        now = time.time()
                        if now - last_status_log > 10:
                            print(f"[status] present={int(is_present)} confidence={fields.get('confidence')} room={fields.get('room')} motion={fields.get('motion')} silhouette={fields.get('silhouette')}")
                            last_status_log = now
                        if present is None:
                            present = is_present
                            if is_present:
                                present_since = time.time()
                                send_transition("HUMAN_PRESENT", fields)
                        elif is_present and not present:
                            present = True
                            present_since = time.time()
                            send_transition("HUMAN_PRESENT", fields)
                        elif not is_present and present:
                            duration = max(0, int(time.time() - present_since))
                            present = False
                            fields = {**fields, "duration": str(duration)}
                            send_transition("HUMAN_LEFT", fields)
                    elif event == "HUMAN_PRESENT":
                        present = True
                        present_since = time.time()
                        send_transition(event, fields)
                    elif event == "HUMAN_LEFT":
                        present = False
                        send_transition(event, fields)
                    time.sleep(0.2)
        except Exception as exc:
            print(f"[serial] disconnected/error: {exc}; retrying in 3s", file=sys.stderr)
            time.sleep(3)


if __name__ == "__main__":
    raise SystemExit(main())
