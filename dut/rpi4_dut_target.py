#!/usr/bin/env python3
"""
rpi4_dut_target.py — DUT (Device Under Test) for Hardware Protocol Fuzzer.

Runs on Raspberry Pi 4 connected to the Pico fuzzer via UART.
Operates as a RAW BYTE STREAM responder — no framing required.
Every received byte/sequence gets processed and triggers a response.

Wiring (RPi4 ↔ Pico):
    RPi4 GPIO14 (TXD / pin 8)  →  Pico RX pin (default: GP5)
    RPi4 GPIO15 (RXD / pin 10) →  Pico TX pin (default: GP4)
    RPi4 GND    (pin 6/14/etc) →  Pico GND

Usage:
    python3 rpi4_dut_target.py                          # /dev/serial0 @ 9600
    python3 rpi4_dut_target.py --port /dev/ttyAMA0      # explicit port
    python3 rpi4_dut_target.py --baud 115200             # match fuzzer baud
    python3 rpi4_dut_target.py --quiet                   # less output
    python3 rpi4_dut_target.py --chaos                  # enable intentional bugs

Behavior:
    The DUT reads raw bytes from the fuzzer and responds based on:
    - First byte (command selector)
    - Byte patterns in the stream
    - Accumulated state (registers, auth flag)

    It replies with 1..N bytes per received chunk so the Pico's capture
    sniffer sees TRACE_DECODED responses correlated with FUZZ_TX stimuli.

Intentional bugs for fuzzer to discover (disabled by default):
    1. Crash magic:   0xDE 0xAD 0xBE 0xEF → target stops responding for 2s
    2. Auth bypass:    0x05 0x00 0x00 0x00 0x00 → unlocks dump command
    3. Buffer overrun: >40 bytes in single chunk to reg 0x0F → canary corrupt
    4. Echo amplify:   0x10 + N bytes → echoes back 2×N bytes (amplification)
    5. Wraparound:     register index >15 silently wraps (mod 16)
"""

import argparse
import os
import signal
import sys
import time
from datetime import datetime
from pathlib import Path

try:
    import serial
except ImportError:
    print("ERROR: pyserial not installed. Run:  pip3 install pyserial")
    sys.exit(1)


# ── Constants ────────────────────────────────────────────────────────

CRASH_MAGIC      = bytes([0xDE, 0xAD, 0xBE, 0xEF])
AUTH_BYPASS_SEQ   = bytes([0x05, 0x00, 0x00, 0x00, 0x00])
AUTH_VALID_PIN    = bytes([0x05, 0x31, 0x33, 0x33, 0x37])  # 0x05 + "1337"
NUM_REGISTERS     = 16
REG_SIZE          = 32
STATS_INTERVAL    = 5.0  # seconds between stats prints


# ── Terminal colors ──────────────────────────────────────────────────

class C:
    RST = "\033[0m";   B = "\033[1m";    DIM = "\033[2m"
    RED = "\033[31m";  GRN = "\033[32m"; YLW = "\033[33m"
    BLU = "\033[34m";  MAG = "\033[35m"; CYN = "\033[36m"


# ── DUT State ────────────────────────────────────────────────────────

class DUTState:
    def __init__(self):
        self.regs = [bytearray(REG_SIZE) for _ in range(NUM_REGISTERS)]
        self.authed = False
        self.canary = bytearray(b"\xCA\xFE" * 8)  # 16-byte integrity canary

        # Counters
        self.rx_bytes    = 0
        self.rx_chunks   = 0
        self.tx_bytes    = 0
        self.ack_count   = 0
        self.nack_count  = 0
        self.crashes     = 0
        self.overflows   = 0
        self.auth_bypass = 0
        self.amplifies   = 0
        self.wraps       = 0
        self.t0          = time.monotonic()

    def reset_regs(self):
        for r in self.regs:
            for i in range(len(r)):
                r[i] = 0
        self.authed = False
        self.canary = bytearray(b"\xCA\xFE" * 8)

    @property
    def canary_ok(self) -> bool:
        return self.canary == bytearray(b"\xCA\xFE" * 8)


# ── CSV Traffic Log ──────────────────────────────────────────────────

class TrafficLog:
    def __init__(self, log_dir: str = "/tmp/dut_logs"):
        os.makedirs(log_dir, exist_ok=True)
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.path = Path(log_dir) / f"dut_{ts}.csv"
        self.f = open(self.path, "w")
        self.f.write("time_ms,dir,hex,resp_hex,tag\n")
        self.t0 = time.monotonic()

    def log(self, direction: str, data: bytes,
            resp: bytes = b"", tag: str = ""):
        ms = int((time.monotonic() - self.t0) * 1000)
        self.f.write(f"{ms},{direction},{data.hex().upper()},"
                     f"{resp.hex().upper()},{tag}\n")

    def close(self):
        self.f.flush()
        self.f.close()


# ── Core: process one chunk of received bytes ────────────────────────

def process_chunk(data: bytes, state: DUTState, ser, log: TrafficLog,
                  verbose: bool, chaos: bool):
    """
    Process a raw chunk of bytes from the fuzzer.
    Respond over UART so the sniffer captures the exchange.
    Returns immediately.
    """
    n = len(data)
    if n == 0:
        return

    state.rx_bytes += n
    state.rx_chunks += 1
    tag = ""

    # ── Bug #1: Crash magic anywhere in chunk ────────────────────
    if chaos and CRASH_MAGIC in data:
        state.crashes += 1
        tag = "CRASH"
        resp = b"\xDE\xAD"
        ser.write(resp)
        state.tx_bytes += len(resp)
        log.log("RX", data, resp, tag)
        if verbose:
            print(f"  {C.RED}{C.B}💥 CRASH #{state.crashes}: "
                  f"0xDEADBEEF in stream!{C.RST}")
        # Simulate hang — stop responding for 2 seconds
        time.sleep(2.0)
        state.reset_regs()
        return

    # ── Bug #2: Auth bypass sequence ─────────────────────────────
    if chaos and AUTH_BYPASS_SEQ in data:
        state.authed = True
        state.auth_bypass += 1
        tag = "AUTH_BYPASS"
        resp = b"\x06\x01"  # ACK + auth OK
        ser.write(resp)
        state.tx_bytes += len(resp)
        state.ack_count += 1
        log.log("RX", data, resp, tag)
        if verbose:
            print(f"  {C.MAG}{C.B}🔓 AUTH BYPASS #{state.auth_bypass}: "
                  f"null PIN accepted!{C.RST}")
        return

    # ── Valid auth ────────────────────────────────────────────────
    if AUTH_VALID_PIN in data:
        state.authed = True
        tag = "AUTH_OK"
        resp = b"\x06\x01"
        ser.write(resp)
        state.tx_bytes += len(resp)
        state.ack_count += 1
        log.log("RX", data, resp, tag)
        if verbose:
            print(f"  {C.GRN}🔑 AUTH OK (valid PIN){C.RST}")
        return

    # ── Dispatch by first byte ───────────────────────────────────
    cmd = data[0]

    # 0x01: PING → PONG
    if cmd == 0x01:
        tag = "PING"
        resp = b"\x06PONG"
        ser.write(resp)
        state.tx_bytes += len(resp)
        state.ack_count += 1

    # 0x02: READ register (2nd byte = index)
    elif cmd == 0x02:
        idx = data[1] if n > 1 else 0
        # Bug #5: wrap silently instead of rejecting
        if chaos and idx >= NUM_REGISTERS:
            state.wraps += 1
            tag = "REG_WRAP"
            idx = idx % NUM_REGISTERS
        elif idx >= NUM_REGISTERS:
            tag = "READ_NACK"
            resp = b"\x15"
            ser.write(resp)
            state.tx_bytes += len(resp)
            state.nack_count += 1
            ser.flush()
            log.log("RX", data, resp, tag)
            if verbose:
                print(f"  {C.YLW}✗ {tag:12s} in={data[:20].hex().upper():42s} out={resp.hex().upper()}{C.RST}")
            return
        else:
            idx = idx % NUM_REGISTERS
        reg_data = bytes(state.regs[idx][:8])  # return first 8 bytes
        resp = b"\x06" + bytes([idx]) + reg_data
        ser.write(resp)
        state.tx_bytes += len(resp)
        state.ack_count += 1
        if not tag:
            tag = "READ"

    # 0x03: WRITE register (2nd byte = index, rest = data)
    elif cmd == 0x03 and n >= 2:
        idx = data[1]
        write_data = data[2:]

        if idx >= NUM_REGISTERS and (not chaos or idx != 0x0F):
            # NACK for out-of-range (but not 0x0F, that's the bug path)
            tag = "WRITE_NACK"
            resp = b"\x15"
            ser.write(resp)
            state.tx_bytes += len(resp)
            state.nack_count += 1
        elif chaos and idx == 0x0F and len(write_data) > REG_SIZE:
            # Bug #3: buffer overrun into canary
            state.overflows += 1
            tag = "OVERFLOW"
            reg = state.regs[0x0F]
            for i, b in enumerate(write_data):
                if i < REG_SIZE:
                    reg[i] = b
                elif (i - REG_SIZE) < len(state.canary):
                    state.canary[i - REG_SIZE] = b
            resp = b"\x06" + bytes([len(write_data) & 0xFF])
            ser.write(resp)
            state.tx_bytes += len(resp)
            state.ack_count += 1
            if verbose:
                print(f"  {C.RED}🔥 OVERFLOW #{state.overflows}: "
                      f"{len(write_data)}B to reg 0x0F, "
                      f"canary={'OK' if state.canary_ok else 'CORRUPTED'}"
                      f"{C.RST}")
        else:
            # Normal write
            idx = idx % NUM_REGISTERS
            wlen = min(len(write_data), REG_SIZE)
            state.regs[idx][:wlen] = write_data[:wlen]
            tag = "WRITE"
            resp = b"\x06" + bytes([wlen])
            ser.write(resp)
            state.tx_bytes += len(resp)
            state.ack_count += 1

    # 0x04: RESET
    elif cmd == 0x04:
        state.reset_regs()
        tag = "RESET"
        resp = b"\x06\x00"
        ser.write(resp)
        state.tx_bytes += len(resp)
        state.ack_count += 1

    # 0x06: DUMP (requires auth)
    elif cmd == 0x06:
        if not state.authed:
            tag = "DUMP_DENIED"
            resp = b"\x15\x06"  # NACK + reason
            ser.write(resp)
            state.tx_bytes += len(resp)
            state.nack_count += 1
        else:
            tag = "DUMP_OK"
            # Send first 4 bytes of each register
            dump = bytearray(b"\x06")
            for r in state.regs:
                dump.extend(r[:4])
            resp = bytes(dump[:64])
            ser.write(resp)
            state.tx_bytes += len(resp)
            state.ack_count += 1

    # 0x07: firmware version
    elif cmd == 0x07:
        tag = "VERSION"
        ver = b"\x06DUT-RPi4-v1.0"
        ser.write(ver)
        state.tx_bytes += len(ver)
        state.ack_count += 1

    # 0x10: ECHO with amplification (Bug #4)
    elif cmd == 0x10:
        echo_data = data[1:] if n > 1 else data
        if chaos:
            # Bug #4: echo back 2× the data (amplification attack surface)
            resp = b"\x06" + echo_data + echo_data
            state.amplifies += 1
            tag = "ECHO_AMP"
        else:
            resp = b"\x06" + echo_data
            tag = "ECHO"
        ser.write(resp)
        state.tx_bytes += len(resp)
        state.ack_count += 1

    # 0xFF: self-test / diagnostics
    elif cmd == 0xFF:
        tag = "SELFTEST"
        diag = bytearray()
        diag.append(0x06)  # ACK
        diag.append(0x01 if state.canary_ok else 0x00)
        diag.append(0x01 if state.authed else 0x00)
        diag.append(state.crashes & 0xFF)
        diag.append(state.overflows & 0xFF)
        diag.append(state.auth_bypass & 0xFF)
        resp = bytes(diag)
        ser.write(resp)
        state.tx_bytes += len(resp)
        state.ack_count += 1

    # Anything else: NACK with echo of first byte
    else:
        tag = "UNKNOWN"
        resp = b"\x15" + bytes([cmd])
        ser.write(resp)
        state.tx_bytes += len(resp)
        state.nack_count += 1

    ser.flush()
    log.log("RX", data, resp, tag)

    if verbose:
        is_ack = resp[0:1] == b"\x06" if resp else False
        color = C.GRN if is_ack else C.YLW
        sym = "→" if is_ack else "✗"
        hex_in = data[:20].hex().upper()
        hex_out = resp[:16].hex().upper()
        extra = ""
        if tag in ("OVERFLOW", "CRASH", "AUTH_BYPASS", "ECHO_AMP", "REG_WRAP"):
            extra = f" {C.RED}{C.B}⚠ {tag}{C.RST}"
        print(f"  {color}{sym} {tag:12s} "
              f"in={hex_in:42s} out={hex_out}{C.RST}{extra}")


# ── Stats printer ────────────────────────────────────────────────────

def print_stats(s: DUTState):
    dt = time.monotonic() - s.t0
    rate = s.rx_chunks / dt if dt > 0 else 0
    print(f"\n{C.B}── Stats ({dt:.0f}s) ─────────────────────────────{C.RST}")
    print(f"  RX: {s.rx_bytes} bytes, {s.rx_chunks} chunks  "
          f"({rate:.0f}/s)  |  TX: {s.tx_bytes} bytes")
    print(f"  ACK: {C.GRN}{s.ack_count}{C.RST}  "
          f"NACK: {C.YLW}{s.nack_count}{C.RST}")

    # Anomalies
    anom = []
    if s.crashes:     anom.append(f"{C.RED}crashes={s.crashes}{C.RST}")
    if s.overflows:   anom.append(f"{C.RED}overflows={s.overflows}{C.RST}")
    if s.auth_bypass: anom.append(f"{C.MAG}auth_bypass={s.auth_bypass}{C.RST}")
    if s.amplifies:   anom.append(f"{C.YLW}echo_amp={s.amplifies}{C.RST}")
    if s.wraps:       anom.append(f"{C.YLW}reg_wrap={s.wraps}{C.RST}")
    if not s.canary_ok:
        anom.append(f"{C.RED}{C.B}CANARY CORRUPTED{C.RST}")

    if anom:
        print(f"  Anomalies: {', '.join(anom)}")
    else:
        print(f"  Anomalies: {C.GRN}none detected{C.RST}")
    print()


# ── Main loop ────────────────────────────────────────────────────────

def run(port: str, baud: int, verbose: bool, chaos: bool):
    print(f"""
{C.CYN}{C.B}╔══════════════════════════════════════════════════════════════╗
║          DUT Target — Hardware Protocol Fuzzer               ║
║          Raspberry Pi 4 UART Responder                       ║
╚══════════════════════════════════════════════════════════════╝{C.RST}

{C.DIM}Mode:  Raw byte-stream (no framing required)
Bugs:  {'enabled' if chaos else 'disabled by default'}
Log:   /tmp/dut_logs/dut_*.csv{C.RST}
""")

    print(f"{C.BLU}Opening {port} at {baud} baud...{C.RST}")
    try:
        ser = serial.Serial(
            port=port, baudrate=baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.01,   # 10ms — fast polling
        )
    except serial.SerialException as e:
        print(f"{C.RED}Failed: {e}{C.RST}")
        sys.exit(1)

    print(f"{C.GRN}✓ Port open. Waiting for fuzzer data...{C.RST}\n")

    state = DUTState()
    log = TrafficLog()
    last_stats = time.monotonic()

    running = True
    def on_signal(sig, frame):
        nonlocal running
        running = False
    signal.signal(signal.SIGINT, on_signal)
    signal.signal(signal.SIGTERM, on_signal)

    try:
        while running:
            try:
                data = ser.read(512)
            except serial.SerialException:
                print(f"{C.RED}Read error, retrying...{C.RST}")
                time.sleep(0.5)
                continue

            if data:
                process_chunk(data, state, ser, log, verbose, chaos)

            # Periodic stats
            now = time.monotonic()
            if now - last_stats >= STATS_INTERVAL and state.rx_chunks > 0:
                print_stats(state)
                last_stats = now

    except Exception as e:
        print(f"\n{C.RED}Fatal: {e}{C.RST}")
    finally:
        print(f"\n{C.CYN}Shutting down...{C.RST}")
        print_stats(state)
        log.close()
        ser.close()
        print(f"{C.GRN}Log: {log.path}{C.RST}")


def main():
    p = argparse.ArgumentParser(
        description="DUT target for Hardware Protocol Fuzzer (RPi4)",
        epilog="""
Wiring:
  RPi4 GPIO14 (TXD/pin 8)  →  Pico RX (default GP5)
  RPi4 GPIO15 (RXD/pin 10) →  Pico TX (default GP4)
  RPi4 GND (pin 6)         →  Pico GND
""",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", default="/dev/serial0",
                   help="Serial port (default: /dev/serial0)")
    p.add_argument("--baud", type=int, default=9600,
                   help="Baud rate — must match fuzzer (default: 9600)")
    p.add_argument("--quiet", action="store_true",
                   help="Suppress per-chunk output")
    p.add_argument("--chaos", action="store_true",
                   help="Enable intentional bug behaviors")
    args = p.parse_args()
    run(args.port, args.baud, verbose=not args.quiet, chaos=args.chaos)


if __name__ == "__main__":
    main()
