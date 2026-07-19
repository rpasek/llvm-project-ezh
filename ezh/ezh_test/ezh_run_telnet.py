# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Load + ignite an EZH ELF over an OpenOCD telnet console and time it to
# completion. Bypasses lldb (slow startup) so the two-board I2C speed sweep can
# iterate fast and timestamp transactions precisely.
#
#   python3 ezh_run_telnet.py <telnet_port> <elf|--noload> <start_hex> <exc_hex>
#                             [timeout_s] [extra_read_hex ...]
# Prints: EXC=0x........ TIME=secs  [READ:0x....=0x........ ...]

import socket, sys, time, re


class OCD:
    def __init__(self, port):
        self.s = socket.create_connection(("127.0.0.1", port), timeout=5)
        self.s.settimeout(0.4)
        self._drain(0.2)

    def _drain(self, t=0.05):
        time.sleep(t)
        out = b""
        try:
            while True:
                d = self.s.recv(4096)
                if not d:
                    break
                out += d
        except socket.timeout:
            pass
        return out.decode(errors="replace")

    def cmd(self, c, settle=0.15):
        self.s.sendall((c + "\n").encode())
        return self._drain(settle)

    def mdw(self, addr):
        o = self.cmd("mdw %s" % addr, settle=0.03)
        m = re.search(re.escape(addr.lower()) + r":\s*([0-9a-fA-F]+)", o.lower())
        return int(m.group(1), 16) if m else None


SENTINELS = (0xFFFFFFFF, 0xDEADDEAD, 0x55555555, 0x0DEADB55)


def main():
    port = int(sys.argv[1]); elf = sys.argv[2]
    start = sys.argv[3]; exc = sys.argv[4]
    tmo = float(sys.argv[5]) if len(sys.argv) > 5 else 30.0
    extra = sys.argv[6:]

    o = OCD(port)
    o.cmd("mww 0x40027024 0xC0DE0000")            # halt EZH
    if elf != "--noload":
        o.cmd("load_image %s" % elf, settle=0.6)  # load ELF segments to SRAM
    rst = o.mdw("0x40000010") or 0
    o.cmd("mww 0x40000010 0x%08x" % (rst | (1 << 30)))   # EZH reset
    o.cmd("mww 0x40000010 0x%08x" % (rst & ~(1 << 30)))
    o.cmd("mww %s 0xFFFFFFFF" % exc)              # init exc_signal
    o.cmd("mww 0x40027048 0x00000080")            # clear bitslice
    o.cmd("mww 0x40027024 0xC0DE0000")            # halt
    o.cmd("mww 0x40027020 %s" % start)            # PC = _start

    t0 = time.time()
    o.cmd("mww 0x40027024 0xC0DE0011", settle=0.0)  # ignite
    val = 0xFFFFFFFF
    while time.time() - t0 < tmo:
        v = o.mdw(exc)
        if v is not None:
            val = v
        if val not in SENTINELS:
            break
    dt = time.time() - t0
    reads = " ".join("READ:%s=0x%08x" % (a, (o.mdw(a) or 0)) for a in extra)
    print("EXC=0x%08x TIME=%.4f %s" % (val, dt, reads))


if __name__ == "__main__":
    main()
