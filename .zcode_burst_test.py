#!/usr/bin/env python3
"""Reproduce the terminal-connect outage: connect, fire the terminal's
on-connect request burst with no spacing, then watch state/acks for 90s."""
import socket
import struct
import sys
import threading
import time

HOST, PORT = "192.168.1.120", 7000
MAGIC = 0x53414C41


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
            crc &= 0xFFFF
    return crc


class Client:
    def __init__(self):
        self.s = socket.create_connection((HOST, PORT), timeout=8)
        self.buf = b""
        self.seq = 0
        self.acks = {}
        self.lock = threading.Lock()
        self.stop = threading.Event()
        self.hb = threading.Thread(target=self._hb, daemon=True)
        self.hb.start()
        self.rx = threading.Thread(target=self._rx, daemon=True)
        self.rx.start()

    def _hb(self):
        n = 20000
        while not self.stop.is_set():
            try:
                f = struct.pack("<IBHHBH", MAGIC, 1, 0x00F0, n, 0, 4)
                f += struct.pack("<I", int(time.time() * 1000) & 0xFFFFFFFF)
                f += struct.pack("<H", crc16(f))
                self.s.sendall(f)
                n = (n + 1) & 0xFFFF
            except OSError:
                return
            self.stop.wait(1.5)

    def _rx(self):
        while not self.stop.is_set():
            try:
                chunk = self.s.recv(8192)
                if not chunk:
                    return
                self.buf += chunk
            except OSError:
                return
            while len(self.buf) >= 12:
                magic, ver, fid, sq, fl, ln = struct.unpack("<IBHHBH", self.buf[:12])
                if magic != MAGIC:
                    self.buf = self.buf[1:]
                    continue
                total = 12 + ln + 2
                if len(self.buf) < total:
                    break
                frame, self.buf = self.buf[:total], self.buf[total:]
                if crc16(frame[:-2]) != struct.unpack("<H", frame[-2:])[0]:
                    continue
                if fid == 0x0101:
                    with self.lock:
                        self.acks[sq] = struct.unpack("<H", frame[12:14])[0]

    def send(self, fid, payload=b""):
        with self.lock:
            self.seq = (self.seq + 1) & 0xFFFF
            sq = self.seq
        f = struct.pack("<IBHHBH", MAGIC, 1, fid, sq, 1, len(payload)) + payload
        f += struct.pack("<H", crc16(f))
        self.s.sendall(f)
        return sq

    def result(self, sq, timeout):
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self.lock:
                if sq in self.acks:
                    return self.acks.pop(sq)
            time.sleep(0.02)
        return None

    def close(self):
        self.stop.set()
        try:
            self.s.close()
        except OSError:
            pass


# Terminal-like on-connect burst (no spacing between sends)
BURST = [
    (0x0001, b""),            # ask
    (0x0002, b""),            # ver
    (0x0034, b""),            # get servo all
    (0x0043, struct.pack("<B", 10)),  # get propeller base CH10
    (0x0043, struct.pack("<B", 14)),
    (0x0044, struct.pack("<B", 10)),  # get propeller real
    (0x0044, struct.pack("<B", 14)),
    (0x0062, b""),            # sensor all
    (0x0061, b""),            # sensor dyp
    (0x0060, b""),            # sensor mpu
    (0x0033, struct.pack("<B", 0)),   # get servo CH0
    (0x0030, struct.pack("<BH", 0, 90)),  # set servo CH0 (current angle)
    (0x0010, b""),            # stop all (safe)
]

c = Client()
print("connected; firing burst of", len(BURST))
seqs = [(fid, c.send(fid, pl)) for fid, pl in BURST]
print("burst sent; collecting acks (2s window)...")
timeout_count = 0
for fid, sq in seqs:
    r = c.result(sq, 2.0)
    if r is None:
        timeout_count += 1
        print(f"  TIMEOUT  fid=0x{fid:04X} seq={sq}")
    elif r != 0:
        print(f"  err      fid=0x{fid:04X} ack=0x{r:04X}")
print(f"burst result: {len(BURST) - timeout_count}/{len(BURST)} acked")

print("watching for 90s: mpu query every 3s + dyp every 10s ...")
t0 = time.time()
fails = 0
while time.time() - t0 < 90:
    sq = c.send(0x0060)
    if c.result(sq, 2.0) is None:
        fails += 1
        print(f"  t+{time.time()-t0:5.1f}s mpu TIMEOUT")
    if int(time.time() - t0) % 10 == 3:
        sq = c.send(0x0061)
        if c.result(sq, 2.0) is None:
            print(f"  t+{time.time()-t0:5.1f}s dyp TIMEOUT")
    time.sleep(3.0)
print(f"90s window: mpu timeouts={fails}")
c.close()
