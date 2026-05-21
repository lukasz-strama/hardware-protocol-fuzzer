import sys
import serial
import time

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM9"

print(f"Otwieram {PORT}")
ser = serial.Serial(PORT, timeout=0.1)
time.sleep(0.5)
ser.reset_input_buffer()

print("cczytanie bajtów\n")
start = time.time()
total = 0

while time.time() - start < 15:
    data = ser.read(64)
    if data:
        total += len(data)
        print(f"[{time.time()-start:.1f}s] {len(data)} bajtów: {data.hex()} | {repr(data)}")

print(f"\nW sumie: {total} ")
ser.close()