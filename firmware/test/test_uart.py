import sys
from pico_sniffer import PicoSniffer

PORT     = sys.argv[1] if len(sys.argv) > 1 else "COM9"
PIN      = 17      
BAUDRATE = 9600  
SECONDS  = 10       

print(f"Pico na {PORT}")

with PicoSniffer(PORT) as sniffer:
    print(f"Połączono! Nasłuchuję UART GP{PIN} @ {BAUDRATE} przez {SECONDS}s...\n")
    sniffer.start_uart(pin=PIN, baudrate=BAUDRATE)

    for line in sniffer.capture_lines(seconds=SECONDS):
        print(f"  >> {line}")

print("\nGotowe.")