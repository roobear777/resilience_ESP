#!/usr/bin/env python3
"""Guided California LED Output Expander validation over ESP32 USB Serial."""

from __future__ import annotations

import argparse
import datetime as _dt
import sys
import time
from pathlib import Path

SCRIPT_NAME = "validate_led_expander.py"
SCRIPT_VERSION = "1.0"
USB_SERIAL_BAUD = 115200
LOG_DIR = Path("validation_logs")

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("Missing Python dependency: pyserial")
    print("Install it with:")
    print()
    print("python3 -m pip install pyserial")
    sys.exit(1)


class ValidationLog:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.lines: list[str] = []

    def add(self, text: str = "") -> None:
        timestamp = _dt.datetime.now().strftime("%H:%M:%S")
        self.lines.append(f"[{timestamp}] {text}" if text else "")

    def write(self) -> None:
        self.path.write_text("\n".join(self.lines) + "\n", encoding="utf-8")


def make_log_path() -> Path:
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    stamp = _dt.datetime.now().strftime("%Y-%m-%d_%H%M%S")
    candidate = LOG_DIR / f"led_expander_validation_{stamp}.txt"

    if not candidate.exists():
        return candidate

    suffix = 2
    while True:
        candidate = LOG_DIR / f"led_expander_validation_{stamp}_{suffix}.txt"
        if not candidate.exists():
            return candidate
        suffix += 1


def print_heading() -> None:
    print()
    print("Tardi LED Output Expander Validation")
    print("=" * 38)
    print()
    print("This script does not upload firmware.")
    print("Zael must already have uploaded the California validation firmware.")
    print("The ESP32 should boot with LED mode OFF.")
    print("Real LEDs should not start animating on power-up.")
    print("Each test gate opens only after Zael confirms the previous step worked.")
    print()


def likely_ports() -> list[str]:
    ports = []
    for port in list_ports.comports():
        device = port.device
        if device.startswith("/dev/cu.usbmodem") or device.startswith("/dev/cu.usbserial"):
            ports.append(device)
    return sorted(ports)


def choose_port(provided_port: str | None) -> str:
    if provided_port:
        return provided_port

    ports = likely_ports()
    if not ports:
        print("No likely Mac USB Serial ports found.")
        return input("Enter ESP32 Serial port, for example /dev/cu.usbserial-0001: ").strip()

    print("Likely ESP32 USB Serial ports:")
    for i, port in enumerate(ports, start=1):
        print(f"  {i}. {port}")

    if len(ports) == 1:
        answer = input(f"Use {ports[0]}? [Y/n]: ").strip().lower()
        if answer in ("", "y", "yes"):
            return ports[0]

    while True:
        answer = input("Choose port number, or paste a port path: ").strip()
        if answer.isdigit():
            index = int(answer)
            if 1 <= index <= len(ports):
                return ports[index - 1]
        if answer:
            return answer
        print("Please choose a port.")


def read_response(ser: serial.Serial, total_timeout: float = 2.0, quiet_timeout: float = 0.35) -> str:
    deadline = time.monotonic() + total_timeout
    quiet_deadline = time.monotonic() + quiet_timeout
    chunks: list[bytes] = []

    while time.monotonic() < deadline:
        waiting = ser.in_waiting
        if waiting:
            chunks.append(ser.read(waiting))
            quiet_deadline = time.monotonic() + quiet_timeout
        elif chunks and time.monotonic() >= quiet_deadline:
            break
        else:
            time.sleep(0.05)

    return b"".join(chunks).decode("utf-8", errors="replace").strip()


def drain_background_serial(ser: serial.Serial) -> str:
    if not ser.in_waiting:
        return ""

    return ser.read(ser.in_waiting).decode("utf-8", errors="replace").strip()


def send_command(ser: serial.Serial, log: ValidationLog, command: str) -> str:
    background = drain_background_serial(ser)
    if background:
        log.add("BACKGROUND SERIAL BEFORE COMMAND:")
        for line in background.splitlines():
            log.add(f"  {line}")

    print(f"> {command}")
    log.add(f"COMMAND: {command}")
    ser.write((command + "\n").encode("utf-8"))
    ser.flush()
    response = read_response(ser)
    if response:
        print(f"Output after '{command}' follows.")
        print("(It may include normal background button/FIRE diagnostics.)")
        print(response)
        log.add(f"OUTPUT AFTER COMMAND '{command}':")
        for line in response.splitlines():
            log.add(f"  {line}")
    else:
        print("(no response captured)")
        log.add(f"OUTPUT AFTER COMMAND '{command}': (no response captured)")
    print()
    return response


def ask_yes_no(prompt: str, default: bool | None = None) -> bool:
    if default is True:
        suffix = " [Y/n]: "
    elif default is False:
        suffix = " [y/N]: "
    else:
        suffix = " [y/n]: "

    while True:
        answer = input(prompt + suffix).strip().lower()
        if not answer and default is not None:
            return default
        if answer in ("y", "yes"):
            return True
        if answer in ("n", "no"):
            return False
        print("Please answer y or n.")


def ask_notes(prompt: str = "Notes: ") -> str:
    return input(prompt).strip()


def record_answer(log: ValidationLog, step: str, result: str, notes: str = "") -> None:
    log.add(f"{step}: {result}")
    if notes:
        log.add(f"{step} notes: {notes}")


def status_looks_blocked(status: str) -> bool:
    lower = status.lower()
    blocked_markers = (
        "allowed=0",
        "allowed: 0",
        "allowed false",
        "allowed=false",
        "real output blocked",
        "enable_real_pb_expander_output=false",
    )
    return any(marker in lower for marker in blocked_markers)


def stop_with_failure(ser: serial.Serial, log: ValidationLog, reason: str) -> bool:
    print()
    print(f"STOP: {reason}")
    log.add(f"FINAL RESULT: FAIL - {reason}")
    try:
        send_command(ser, log, "led off")
    except Exception as exc:  # noqa: BLE001 - keep validation shutdown best-effort.
        print(f"Could not send led off: {exc}")
        log.add(f"led off during failure failed: {exc}")
    return False


def run_validation(ser: serial.Serial, log: ValidationLog) -> bool:
    print("Initial status check")
    status = send_command(ser, log, "led status")
    log.add("Initial led status result captured above.")

    if status_looks_blocked(status):
        print("Real output appears to be blocked.")
        print("This may be the normal India-safe build, not the California validation build.")
        log.add("WARNING: status appears to show real output is blocked.")
        if ask_yes_no("Stop now?", default=True):
            record_answer(log, "Blocked guard prompt", "STOP")
            log.add("FINAL RESULT: STOPPED - real output blocked")
            return False
        record_answer(log, "Blocked guard prompt", "CONTINUE")

    print("A. Safe boot check")
    if not ask_yes_no("Did real LEDs stay off at boot?", default=None):
        notes = ask_notes("What happened at boot? ")
        record_answer(log, "Safe boot LEDs stayed off", "FAIL", notes)
        return stop_with_failure(ser, log, "LEDs started unexpectedly at boot")
    record_answer(log, "Safe boot LEDs stayed off", "PASS")

    if not ask_yes_no("Does OLED/Serial state look sensible?", default=None):
        notes = ask_notes("What looked wrong? ")
        record_answer(log, "OLED/Serial startup state", "FAIL", notes)
        return stop_with_failure(ser, log, "OLED/Serial startup state did not look sensible")
    record_answer(log, "OLED/Serial startup state", "PASS")

    print()
    print("B. Solid test")
    send_command(ser, log, "led solid")
    if not ask_yes_no("Did connected LED zones show a dim simple test output?", default=None):
        notes = ask_notes("What happened? ")
        record_answer(log, "Solid test", "FAIL", notes)
        return stop_with_failure(ser, log, "solid test failed")
    record_answer(log, "Solid test", "PASS")

    print()
    print("C. Channel tests")
    for channel in range(8):
        step = f"Channel {channel} test"
        send_command(ser, log, f"led ch {channel}")
        zone = input(f"Which physical zone lit for channel {channel}? ").strip()
        log.add(f"{step} physical zone lit: {zone or '(none entered)'}")

        if not ask_yes_no(f"Did only the expected physical zone for channel {channel} light?", default=None):
            notes = ask_notes("Record wrong/no/multiple-zone details: ")
            record_answer(log, step, "FAIL", notes)
            return stop_with_failure(ser, log, f"channel {channel} zone check failed")

        if not ask_yes_no("Did colours look sensible?", default=None):
            notes = ask_notes("Record colour issue: ")
            record_answer(log, step, "FAIL", notes)
            return stop_with_failure(ser, log, f"channel {channel} colour check failed")

        record_answer(log, step, "PASS")

    print()
    print("D. Animation test")
    if ask_yes_no("Solid and channel tests passed. Run animation now?", default=False):
        send_command(ser, log, "led animation")
        if ask_yes_no("Did animation look sensible?", default=None):
            record_answer(log, "Animation test", "PASS")
        else:
            notes = ask_notes("What looked wrong? ")
            record_answer(log, "Animation test", "FAIL", notes)
            return stop_with_failure(ser, log, "animation test failed")
    else:
        record_answer(log, "Animation test", "SKIP")

    print()
    print("E. Off test")
    send_command(ser, log, "led off")
    if not ask_yes_no("Did active LED output stop?", default=None):
        notes = ask_notes("What stayed on or looked wrong? ")
        record_answer(log, "Off test", "FAIL", notes)
        log.add("FINAL RESULT: FAIL - off test failed")
        return False

    record_answer(log, "Off test", "PASS")
    log.add("FINAL RESULT: PASS")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description="Guided Tardi LED Output Expander validation")
    parser.add_argument("--port", help="ESP32 USB Serial port, for example /dev/cu.usbserial-0001")
    args = parser.parse_args()

    print_heading()
    port = choose_port(args.port)
    if not port:
        print("No serial port selected.")
        return 2

    log_path = make_log_path()
    log = ValidationLog(log_path)
    now = _dt.datetime.now().astimezone()
    log.add(f"Date/time: {now.isoformat()}")
    log.add(f"Script: {SCRIPT_NAME} version {SCRIPT_VERSION}")
    log.add(f"Selected serial port: {port}")
    log.add(f"USB Serial baud: {USB_SERIAL_BAUD}")
    log.add(f"Log path: {log_path}")
    log.add()

    exit_code = 1
    try:
        with serial.Serial(port, USB_SERIAL_BAUD, timeout=0.2, write_timeout=2.0) as ser:
            time.sleep(2.0)
            ser.reset_input_buffer()
            ok = run_validation(ser, log)
            exit_code = 0 if ok else 1
    except serial.SerialException as exc:
        print(f"Serial error: {exc}")
        log.add(f"FINAL RESULT: SERIAL ERROR - {exc}")
    except KeyboardInterrupt:
        print()
        print("Stopped by user.")
        log.add("FINAL RESULT: STOPPED BY USER")
    finally:
        log.add(f"Exact log path: {log_path}")
        log.write()
        print()
        print(f"Validation log saved to: {log_path}")
        print()
        print("Please send this .txt file back to Roopert.")
        print()
        print("open validation_logs")

    return exit_code


if __name__ == "__main__":
    sys.exit(main())
