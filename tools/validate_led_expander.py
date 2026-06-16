#!/usr/bin/env python3
"""Guided California LED Output Expander validation over ESP32 USB Serial."""

from __future__ import annotations

import argparse
import datetime as _dt
import re
import sys
import time
from pathlib import Path

SCRIPT_NAME = "validate_led_expander.py"
SCRIPT_VERSION = "1.4"
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
    print("Real sculpture LEDs connected to the Output Expander should not flicker or animate on power-up.")
    print("Note: firmware 'Outputs=ON' means FIRE outputs are enabled; it does not mean LED UART output is on.")
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


def is_background_diagnostic_line(line: str) -> bool:
    stripped = line.strip()
    return stripped.startswith("Buttons: ") and " | Fire: " in stripped


def is_concise_summary_line(line: str) -> bool:
    stripped = line.strip()
    return (
        stripped.startswith("LED mode")
        or stripped.startswith("LED real output blocked")
        or stripped.startswith("EXP REAL ")
        or stripped.startswith("EXP SIM ")
        or stripped.startswith("Use: led")
        or stripped.startswith("LED commands:")
        or stripped.startswith("Unknown LED command")
    )


def parse_key_values(line: str) -> dict[str, str]:
    return dict(re.findall(r"([A-Za-z]+)=([^ \r\n]+)", line))


def find_last_line(response: str, prefix: str) -> str:
    matches = [line.strip() for line in response.splitlines() if line.strip().startswith(prefix)]
    return matches[-1] if matches else ""


def yes_no_text(value: str) -> str:
    if value == "1":
        return "yes"
    if value == "0":
        return "no"
    return "unknown"


def print_status_summary(response: str) -> bool:
    exp_real_line = find_last_line(response, "EXP REAL ")
    exp_sim_line = find_last_line(response, "EXP SIM ")

    if not exp_real_line and not exp_sim_line:
        return False

    real = parse_key_values(exp_real_line)
    sim = parse_key_values(exp_sim_line)
    sim_state = "UNKNOWN"
    if exp_sim_line.startswith("EXP SIM OK"):
        sim_state = "OK"
    elif exp_sim_line.startswith("EXP SIM FAIL"):
        sim_state = "FAIL"

    print("Initial status summary:")
    if real:
        print(f"  Real output allowed: {yes_no_text(real.get('allowed', '?'))}")
        print(f"  Real output started: {yes_no_text(real.get('started', '?'))}")
        print(f"  TX pin: GPIO{real.get('tx', '?')}")
        print(f"  Output Expander baud: {real.get('baud', '?')}")
    else:
        print("  Real output status: not found")

    if sim:
        print(
            "  Simulator: "
            f"{sim_state}, "
            f"{sim.get('channels', '?')} channels, "
            f"{sim.get('pixels', '?')} pixels, "
            f"{sim.get('failed', '?')} failed"
        )
    else:
        print("  Simulator: not found")

    return True


def print_command_response_summary(command: str, response: str) -> None:
    lines = response.splitlines()
    background_count = sum(1 for line in lines if is_background_diagnostic_line(line))

    if command == "led status" and print_status_summary(response):
        if background_count:
            print(f"({background_count} repeated button/FIRE diagnostic line(s) hidden here; full raw Serial is in the log.)")
        return

    summary_lines = [line for line in lines if is_concise_summary_line(line)]

    if summary_lines:
        print(f"Summary after '{command}':")
        for line in summary_lines:
            print(line)
    else:
        visible_lines = [line for line in lines if line.strip() and not is_background_diagnostic_line(line)]
        if visible_lines:
            print(f"Output after '{command}' follows:")
            for line in visible_lines[:8]:
                print(line)
            if len(visible_lines) > 8:
                print(f"... {len(visible_lines) - 8} more line(s) hidden here; full raw Serial is in the log.")
        else:
            print(f"(no command-specific response found after '{command}')")

    if background_count:
        print(f"({background_count} repeated button/FIRE diagnostic line(s) hidden here; full raw Serial is in the log.)")


def send_command(ser: serial.Serial, log: ValidationLog, command: str) -> str:
    background = drain_background_serial(ser)
    if background:
        log.add("RAW BACKGROUND SERIAL BEFORE COMMAND:")
        for line in background.splitlines():
            log.add(f"  {line}")

    print(f"> {command}")
    log.add(f"COMMAND: {command}")
    ser.write((command + "\n").encode("utf-8"))
    ser.flush()
    response = read_response(ser)
    if response:
        print_command_response_summary(command, response)
        log.add(f"RAW OUTPUT AFTER COMMAND '{command}':")
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


def run_colour_order_tests(ser: serial.Serial, log: ValidationLog) -> bool:
    print()
    print("D. Colour-order tests")
    print("These low-brightness tests confirm red, green, and blue are not swapped.")

    tests = (
        ("red", "RED"),
        ("green", "GREEN"),
        ("blue", "BLUE"),
    )

    for command_colour, expected_colour in tests:
        step = f"Colour test {expected_colour}"
        send_command(ser, log, f"led {command_colour}")
        log.add(f"{step} expected colour: {expected_colour}")

        if ask_yes_no(f"Did the LEDs look {expected_colour.lower()}?", default=None):
            record_answer(log, step, "PASS")
            continue

        actual_colour = ask_notes("What colour did they actually show? ")
        notes = ask_notes("Any extra notes about the colour mismatch? ")
        mismatch_notes = f"actual={actual_colour or '(not entered)'}"
        if notes:
            mismatch_notes += f"; notes={notes}"
        record_answer(log, step, "FAIL", mismatch_notes)
        return stop_with_failure(ser, log, f"{expected_colour.lower()} colour-order test failed")

    return True


def run_validation(ser: serial.Serial, log: ValidationLog) -> bool:
    print("A. Boot/reset flicker check")
    print("In this check, LEDs means the real LED strings connected to the Output Expander.")
    print("This does not mean the ESP32 onboard LEDs.")
    print("India USB-only dry run: if no Output Expander and no real LED strings are connected, answer n.")
    flickered = ask_yes_no(
        "With the Output Expander connected and real LED strings powered, reset or power-cycle the ESP32. "
        "Before this script sends any led command, did any real sculpture LEDs flicker, flash, or animate?",
        default=None,
    )
    if flickered:
        notes = ask_notes("Which channels/zones flickered, flashed, or animated? ")
        record_answer(log, "Boot/reset flicker check", "FAIL", notes)
        return stop_with_failure(ser, log, "real sculpture LEDs flickered before any led command")
    record_answer(log, "Boot/reset flicker check", "PASS")

    print()
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

    print("B. Setup/status check")
    if not ask_yes_no("Does OLED/Serial state look sensible?", default=None):
        notes = ask_notes("What looked wrong? ")
        record_answer(log, "OLED/Serial startup state", "FAIL", notes)
        return stop_with_failure(ser, log, "OLED/Serial startup state did not look sensible")
    record_answer(log, "OLED/Serial startup state", "PASS")

    print()
    print("C. Solid test")
    send_command(ser, log, "led solid")
    if not ask_yes_no("Did connected LED zones show a dim simple test output?", default=None):
        notes = ask_notes("What happened? ")
        record_answer(log, "Solid test", "FAIL", notes)
        return stop_with_failure(ser, log, "solid test failed")
    record_answer(log, "Solid test", "PASS")

    print()
    if not run_colour_order_tests(ser, log):
        return False

    print()
    print("E. Channel tests")
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
    print("F. Animation test")
    if ask_yes_no("Solid, colour-order, and channel tests passed. Run animation now?", default=False):
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
    print("G. Off test")
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
        if "resource busy" in str(exc).lower():
            print("Close Arduino Serial Monitor / Serial Plotter, then run this script again.")
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
