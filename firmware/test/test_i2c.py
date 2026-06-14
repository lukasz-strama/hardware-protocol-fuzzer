import sys, serial, struct, time

PORT     = sys.argv[1] if len(sys.argv) > 1 else "COM9"
SDA_PIN  = int(sys.argv[2]) if len(sys.argv) > 2 else 4   
SCL_PIN  = SDA_PIN + 1                                     
I2C_FREQ = 10_000                                         

MAGIC = b'\x55\xAA'

# Typy wiadomości
MSG_HELLO     = 0x01
MSG_HELLO_ACK = 0x02
MSG_STATUS    = 0x13
MSG_SET_BUS   = 0x20
MSG_ARM       = 0x30
MSG_ARM_OK    = 0x31
MSG_START     = 0x40
MSG_TRACE     = 0x41
MSG_STOP      = 0x50
MSG_STOP_OK   = 0x51
MSG_ERROR     = 0x70

# Typy zdarzeń w TRACE
EVENT_NAMES = {
    0: "BYTE", 1: "START", 2: "STOP",
    3: "ACK",  4: "NACK",  5: "BREAK", 6: "OVERFLOW"
}

BUS_I2C  = 0
BUS_UART = 1


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = (crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1
        crc &= 0xFFFF
    return crc


def send(ser, msg_type, payload=b'', session_id=0, seq=0):
    length = len(payload)
    hdr_no_crc = struct.pack('<2sBBBBHIHH', MAGIC, 1, msg_type, 0, 0,
                             session_id, seq, length, 0)
    crc = crc16(hdr_no_crc[:14] + payload)
    hdr = struct.pack('<2sBBBBHIHH', MAGIC, 1, msg_type, 0, 0,
                     session_id, seq, length, crc)
    ser.write(hdr + payload)


def read_frame(ser, timeout=3.0):
    deadline = time.time() + timeout
    buf = b''
    while time.time() < deadline:
        b = ser.read(1)
        if not b:
            continue
        buf += b
        if len(buf) >= 2 and buf[-2:] == MAGIC:
            break
    else:
        return None
    raw = ser.read(14)
    if len(raw) < 14:
        return None
    header = MAGIC + raw
    _, _, msg_type, _, _, session_id, seq, length, _ = struct.unpack('<2sBBBBHIHH', header)
    payload = ser.read(length) if length > 0 else b''
    return msg_type, session_id, seq, payload


def wait_for(ser, expected, timeout=3.0, label=""):
    deadline = time.time() + timeout
    while time.time() < deadline:
        r = read_frame(ser, timeout=0.5)
        if r is None:
            continue
        if r[0] == expected:
            return r
        if r[0] == MSG_ERROR:
            print(f"  [!] Firmware błąd (type=0x70)")
        if r[0] == MSG_STATUS:
            state = struct.unpack('<I', r[3][8:12])[0] if len(r[3]) >= 12 else "?"
            print(f"  [i] STATUS od Pico, stan sesji = {state}")
    print(f"  [BŁĄD] Timeout oczekując {label} (0x{expected:02X})")
    return None


# POŁĄCZENIE
print(f"Łączę z {PORT}…")
ser = serial.Serial(PORT, timeout=0.5)
time.sleep(0.3)
ser.reset_input_buffer()
seq = 0

# HELLO
print("\n1. HELLO")
hello = struct.pack('<BBHxx', 1, 0x04, 0x0100)
send(ser, MSG_HELLO, hello, session_id=0, seq=seq); seq += 1
r = wait_for(ser, MSG_HELLO_ACK, label="HELLO_ACK")
if not r:
    sys.exit(1)
session_id = r[1]
print(f"   OK — session_id = 0x{session_id:04X}")

# SET_BUS I2C 
print(f"\n2. SET_BUS - I2C, {I2C_FREQ} Hz, SDA=GP{SDA_PIN}, SCL=GP{SCL_PIN}")
# struct hw_protocol_set_bus: speed_hz(u32), bus_type(u8), bus_flags(u8), pin_a(u8), pin_b(u8), reserved(u32)
set_bus = struct.pack('<IBBBBI', I2C_FREQ, BUS_I2C, 0, SDA_PIN, SCL_PIN, 0)
send(ser, MSG_SET_BUS, set_bus, session_id, seq); seq += 1
time.sleep(0.15)

# ARM 
print("\n3. ARM")
arm = struct.pack('<HBB', session_id, 0, 0)
send(ser, MSG_ARM, arm, session_id, seq); seq += 1
r = wait_for(ser, MSG_ARM_OK, label="ARM_OK")
if not r:
    sys.exit(1)
print("   OK — gotowy do przechwytywania")

# START
print("\n4. START CAPTURE")
send(ser, MSG_START, b'', session_id, seq); seq += 1
print("   Przechwytywanie I2C przez 10 sekund…")
print("   (teraz wykonaj jakąś komunikację I2C na magistrali)\n")

# ODBIÓR TRACE
deadline = time.time() + 10.0
trace_count = 0
frame_bytes = bytearray()

while time.time() < deadline:
    r = read_frame(ser, timeout=0.2)
    if r is None:
        continue

    mtype, _, _, payload = r

    if mtype == MSG_TRACE:
        trace_count += 1
        if len(payload) < 12:
            continue
        trace_seq, ts_us, data_len, src_bus, event_type = struct.unpack('<IIHBB', payload[:12])
        data = payload[12:12 + data_len]
        event_name = EVENT_NAMES.get(event_type, f"0x{event_type:02X}")

        if event_type == 1:   # START
            frame_bytes.clear()
            print(f"  [{ts_us:>10} µs]  ── START ──")
        elif event_type == 2: # STOP
            print(f"  [{ts_us:>10} µs]  ── STOP ── (ramka: {frame_bytes.hex()})")
            frame_bytes.clear()
        elif event_type == 0: # BYTE
            b = data[0] if data else 0
            frame_bytes.append(b)
            print(f"  [{ts_us:>10} µs]  BYTE  0x{b:02X}  ({b:3d})", end="")
            if trace_count == 1:
                print(f"  ← adres urządzenia 0x{b>>1:02X} ({'zapis' if b&1==0 else 'odczyt'})", end="")
            print()
        elif event_type == 3: # ACK
            print(f"  [{ts_us:>10} µs]  ACK")
        elif event_type == 4: # NACK
            print(f"  [{ts_us:>10} µs]  NACK  ← urządzenie nie odpowiedziało!")
        elif event_type == 6: # OVERFLOW
            print(f"  [{ts_us:>10} µs]  [!] OVERFLOW — bufor przepełniony")
        else:
            print(f"  [{ts_us:>10} µs]  {event_name}  {data.hex()}")

    elif mtype == MSG_ERROR:
        print("  [!] Firmware zgłosił błąd")
    elif mtype == MSG_STATUS:
        pass  # ignoruj status pings

# STOP 
print(f"\n5. STOP (przechwycono {trace_count} zdarzeń)")
send(ser, MSG_STOP, b'', session_id, seq); seq += 1
wait_for(ser, MSG_STOP_OK, timeout=2.0, label="STOP_OK")
ser.close()

if trace_count == 0:
    print("""
  Nie przechwycono żadnych danych I2C""")
else:
    print(f"\n  I2C sniffer działa poprawnie")