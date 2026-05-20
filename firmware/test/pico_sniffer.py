import serial
import struct
import time
from dataclasses import dataclass
from typing import Iterator, Optional

_MAGIC           = b'\x55\xAA'
_VERSION         = 1

_MSG_HELLO       = 0x01
_MSG_HELLO_ACK   = 0x02
_MSG_SET_BUS     = 0x20
_MSG_ARM         = 0x30
_MSG_ARM_OK      = 0x31
_MSG_START       = 0x40
_MSG_TRACE       = 0x41
_MSG_STOP        = 0x50
_MSG_STOP_OK     = 0x51
_MSG_RESET       = 0x61
_MSG_ERROR       = 0x70

_BUS_UART        = 1
_EVENT_BYTE      = 0
_EVENT_OVERFLOW  = 6


# ---------------------------------------------------------------------------
# Publiczne typy danych
# ---------------------------------------------------------------------------

@dataclass
class TraceFrame:
   
    sequence:     int  
    timestamp_us: int    
    data:         bytes 
    overflow:     bool   


class SnifferError(Exception):
    pass



class PicoSniffer:
   

    def __init__(self, port: str, timeout: float = 3.0):
   
        self._port_name = port
        self._timeout   = timeout
        self._serial:    Optional[serial.Serial] = None
        self._session_id = 0
        self._seq        = 0


    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *_):
        try:
            self.stop()
        except Exception:
            pass
        self.disconnect()

    
    def connect(self):
        
        self._serial = serial.Serial(self._port_name, timeout=self._timeout)
        time.sleep(0.1)
        self._serial.reset_input_buffer()
        self._hello()

    def disconnect(self):
  
        if self._serial and self._serial.is_open:
            self._serial.close()

    def start_uart(self, pin: int, baudrate: int = 115200):
        
        self._set_bus_uart(pin=pin, baudrate=baudrate)
        self._arm()
        self._start_capture()

    def stop(self):
        """Zatrzymuje przechwytywanie."""
        self._send(_MSG_STOP)
        try:
            self._wait_for(_MSG_STOP_OK, timeout=2.0)
        except SnifferError:
            pass


    def capture(self, seconds: float = 10.0) -> Iterator[TraceFrame]:
    
        deadline = time.time() + seconds
        while time.time() < deadline:
            try:
                msg_type, _, _, payload = self._read_frame(timeout=0.2)
            except TimeoutError:
                continue

            if msg_type == _MSG_TRACE:
                frame = self._parse_trace(payload)
                if frame:
                    yield frame
            elif msg_type == _MSG_ERROR:
                msg = payload[8:].decode("utf-8", errors="replace") if len(payload) > 8 else "unknown"
                raise SnifferError(f"Firmware error: {msg}")

    def capture_lines(self, seconds: float = 10.0, encoding: str = "utf-8") -> Iterator[str]:
      
        buf = bytearray()
        for frame in self.capture(seconds=seconds):
            buf += frame.data
            while b'\n' in buf:
                line, buf = buf.split(b'\n', 1)
                yield line.rstrip(b'\r').decode(encoding, errors="replace")

   
    def _hello(self):
        payload = struct.pack('<BBHxx', 1, 0x04, 0x0100)
        self._send(_MSG_HELLO, payload, session_id=0)
        msg_type, session_id, _, _ = self._wait_for(_MSG_HELLO_ACK)
        self._session_id = session_id

    def _set_bus_uart(self, pin: int, baudrate: int):
        payload = struct.pack('<IBBBBI', baudrate, _BUS_UART, 0, 0, pin, 0)
        self._send(_MSG_SET_BUS, payload)

    def _arm(self):
        payload = struct.pack('<HBB', self._session_id, 0, 0)
        self._send(_MSG_ARM, payload)
        self._wait_for(_MSG_ARM_OK)

    def _start_capture(self):
        self._send(_MSG_START)

    def _send(self, msg_type: int, payload: bytes = b'', session_id: int = None):
        if session_id is None:
            session_id = self._session_id
        length = len(payload)
        
        header_no_crc = struct.pack('<2sBBBBHIHH',
            _MAGIC, _VERSION, msg_type, 0, 0,
            session_id, self._seq, length, 0)
        crc = _crc16(header_no_crc[:14] + payload)
        header = struct.pack('<2sBBBBHIHH',
            _MAGIC, _VERSION, msg_type, 0, 0,
            session_id, self._seq, length, crc)
        self._serial.write(header + payload)
        self._seq += 1

    def _read_frame(self, timeout: float = None) -> tuple:
        deadline = time.time() + (timeout or self._timeout)
        buf = b''
        while time.time() < deadline:
            b = self._serial.read(1)
            if not b:
                continue
            buf += b
            if len(buf) >= 2 and buf[-2:] == _MAGIC:
                break
        else:
            raise TimeoutError("Timeout waiting for magic bytes")

        raw = self._serial.read(14)
        if len(raw) < 14:
            raise SnifferError("Incomplete header")
        header = _MAGIC + raw
        _, _, msg_type, _, _, session_id, seq, length, _ = struct.unpack('<2sBBBBHIHH', header)
        payload = self._serial.read(length) if length > 0 else b''
        return msg_type, session_id, seq, payload

    def _wait_for(self, expected_type: int, timeout: float = None) -> tuple:
        deadline = time.time() + (timeout or self._timeout)
        while time.time() < deadline:
            try:
                result = self._read_frame(timeout=0.5)
            except TimeoutError:
                continue
            if result[0] == expected_type:
                return result
            if result[0] == _MSG_ERROR:
                payload = result[3]
                msg = payload[8:].decode("utf-8", errors="replace") if len(payload) > 8 else "unknown"
                raise SnifferError(f"Firmware error: {msg}")
        raise SnifferError(f"Timeout waiting for message type 0x{expected_type:02X}")

    def _parse_trace(self, payload: bytes) -> Optional[TraceFrame]:
        if len(payload) < 12:
            return None
        trace_seq, ts_us, data_len, src_bus, event_type = struct.unpack('<IIHBB', payload[:12])
        data = payload[12:12 + data_len]
        return TraceFrame(
            sequence=trace_seq,
            timestamp_us=ts_us,
            data=data,
            overflow=(event_type == _EVENT_OVERFLOW),
        )


def _crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = (crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1
        crc &= 0xFFFF
    return crc