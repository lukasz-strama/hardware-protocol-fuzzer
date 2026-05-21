import sys, serial, struct, time

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM9"
MAGIC = b'\x55\xAA'

def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = (crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1
        crc &= 0xFFFF
    return crc

def send(ser, msg_type, payload=b'', session_id=0, seq=0):
    length = len(payload)
    hdr_no_crc = struct.pack('<2sBBBBHIHH', MAGIC, 1, msg_type, 0, 0, session_id, seq, length, 0)
    crc = crc16(hdr_no_crc[:14] + payload)
    hdr = struct.pack('<2sBBBBHIHH', MAGIC, 1, msg_type, 0, 0, session_id, seq, length, crc)
    frame = hdr + payload
    print(f"  SEND type=0x{msg_type:02X} len={length} crc=0x{crc:04X}")
    print(f"       hex: {frame.hex()}")
    ser.write(frame)

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
    _, _, msg_type, _, _, session_id, seq, length, checksum = struct.unpack('<2sBBBBHIHH', header)
    payload = ser.read(length) if length > 0 else b''
    print(f"  RECV type=0x{msg_type:02X} session_id=0x{session_id:04X} len={length} crc=0x{checksum:04X}")
    print(f"       hex: {(header+payload).hex()}")
    return msg_type, session_id, seq, payload

ser = serial.Serial(PORT, timeout=0.5)
time.sleep(0.3)
ser.reset_input_buffer()
seq = 0

print("1. HELLO")
hello = struct.pack('<BBHxx', 1, 0x04, 0x0100)
send(ser, 0x01, hello, session_id=0, seq=seq); seq+=1
r = read_frame(ser)
if not r: print("   BRAK odpowiedzi!"); sys.exit(1)
session_id = r[1]
print(f"   session_id = 0x{session_id:04X}")

print("\n2. SET_BUS (UART, 9600, pin_b=17)")
set_bus = struct.pack('<IBBBBI', 9600, 1, 0, 0, 17, 0)
send(ser, 0x20, set_bus, session_id, seq); seq+=1
time.sleep(0.1)

print("\n3. ARM")
arm = struct.pack('<HBB', session_id, 0, 0)
send(ser, 0x30, arm, session_id, seq); seq+=1
r = read_frame(ser)
if r: print(f"   ARM_OK state={r[3][2] if len(r[3])>2 else '?'}")
else: print("   BRAK ARM_OK!")

print("\n4. START_CAPTURE")
send(ser, 0x40, b'', session_id, seq); seq+=1
r = read_frame(ser, timeout=1.0)
if r: print(f"   odpowiedź type=0x{r[0]:02X}")

print("\n5. TRACE")
start = time.time()
traces = 0
while time.time() - start < 10:
    r = read_frame(ser, timeout=0.5)
    if r and r[0] == 0x41:
        traces += 1
        payload = r[3]
        if len(payload) >= 12:
            ts, data_len = struct.unpack_from('<II', payload, 4)[0], struct.unpack_from('<H', payload, 8)[0]
            data = payload[12:12+data_len]
            print(f"   TRACE #{traces} ts={ts}us data={data.hex()} '{data}'")

print(f"\nŁącznie trace: {traces}")
ser.close()