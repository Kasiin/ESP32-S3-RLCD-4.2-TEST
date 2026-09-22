"""Bounded USB console capture; never toggles reset or boot control lines."""
import argparse
import pathlib
import sys
import time
import serial

parser = argparse.ArgumentParser()
parser.add_argument("--port", required=True)
parser.add_argument("--seconds", type=float, default=30)
parser.add_argument("--send", default="")
parser.add_argument("--log", default="")
args = parser.parse_args()
port = serial.Serial(port=None, baudrate=115200, timeout=0.2, write_timeout=2)
port.dtr = False
port.rts = False
port.port = args.port
data = bytearray()
sent = False
end = time.monotonic() + args.seconds
try:
    while time.monotonic() < end:
        try:
            if not port.is_open:
                port.open()
            if args.send and not sent:
                port.write(args.send.encode("ascii"))
                port.flush()
                sent = True
            chunk = port.read(max(1, port.in_waiting))
            if chunk:
                data.extend(chunk)
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
        except (serial.SerialException, OSError) as exc:
            print(f"\n[HOST] USB disconnected/unavailable: {exc}", flush=True)
            port.close()
            time.sleep(1)
finally:
    port.close()
    if args.log:
        pathlib.Path(args.log).write_bytes(data)
