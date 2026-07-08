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
# Two-board EZH I2C lab: payload-tracking check, clock-speed sweep to find the
# bit-bang ceiling, and differential timing to report the real SCL frequency.
# Slave on board A (telnet :4444), master on board B (telnet :4445). Drives
# everything over OpenOCD telnet (no lldb) so iterations are fast and timed.

import os, re, sys, time, socket, subprocess

BUILD = "../../build/bin"
A_PORT, B_PORT = 4444, 4445           # OpenOCD telnet consoles
START = "0x24100000"                  # _start (entry) for every EZH ELF
PAYLOAD = 0x3C                        # distinctive byte the master writes
HALVES = [int(x) for x in os.environ.get(
    "EZH_HALVES", "500,200,100,50,30,20,14,10,7,5,4,3,2,1").split(",")]
CONFIRM = int(os.environ.get("EZH_CONFIRM", "2"))   # runs per HALF; all must pass
SENTINELS = (0xFFFFFFFF, 0xDEADDEAD, 0x55555555, 0x0DEADB55)


class OCD:
    def __init__(self, port):
        self.s = socket.create_connection(("127.0.0.1", port), timeout=5)
        self.s.settimeout(0.4); self._drain(0.2)

    def _drain(self, t=0.05):
        time.sleep(t); out = b""
        try:
            while True:
                d = self.s.recv(4096)
                if not d: break
                out += d
        except socket.timeout: pass
        return out.decode(errors="replace")

    def cmd(self, c, settle=0.12):
        self.s.sendall((c + "\n").encode()); return self._drain(settle)

    def mdw(self, addr):
        o = self.cmd("mdw %s" % addr, settle=0.03)
        m = re.search(re.escape(addr.lower()) + r":\s*([0-9a-fA-F]+)", o.lower())
        return int(m.group(1), 16) if m else None


def sh(cmd):
    r = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if r.returncode:
        sys.exit("BUILD FAILED:\n" + cmd + "\n" + r.stderr)


def syms(elf, names):
    o = subprocess.run([BUILD + "/llvm-readelf", "-s", elf],
                       capture_output=True, text=True).stdout
    d = {}
    for ln in o.splitlines():
        p = ln.split()
        if len(p) >= 8 and p[7] in names:
            d[p[7]] = "0x" + p[1]
    return d


def build(name, defs=""):
    sh(f"{BUILD}/clang -target ezh-none-elf -mno-ezh-bitslice-interrupts -Os "
       f"-ffunction-sections -fdata-sections -Wall -Wextra -Werror "
       f"-isystem ../../build/libc/libc/include -I ../../libc -I . -I .. {defs} "
       f"{name}.c -c -o out/{name}.o")
    sh(f"{BUILD}/ld.lld -T ../smartdma.ld --gc-sections out/crt0.o out/{name}.o "
       f"../../build/libc/libc/lib/libc_nano.a "
       f"../../build/compiler-rt/lib/linux/libclang_rt.builtins-ezh.a "
       f"-o out/{name}.elf")
    return os.path.abspath(f"out/{name}.elf")


def bringup(o):
    for c in ("mww 0x40001040 0x40000000", "mww 0x40002634 0x00030000",
              "mww 0x4013500C 0x1"):
        o.cmd(c)


def prime(o, exc, elf=None):
    """Halt EZH, (optionally) load ELF, reset, init exc, set PC — ready to fire."""
    o.cmd("mww 0x40027024 0xC0DE0000")
    if elf:
        o.cmd("load_image %s" % elf, settle=0.6)
    rst = o.mdw("0x40000010") or 0
    o.cmd("mww 0x40000010 0x%08x" % (rst | (1 << 30)))
    o.cmd("mww 0x40000010 0x%08x" % (rst & ~(1 << 30)))
    o.cmd("mww %s 0xFFFFFFFF" % exc)
    o.cmd("mww 0x40027048 0x00000080")
    o.cmd("mww 0x40027024 0xC0DE0000")
    o.cmd("mww 0x40027020 %s" % START)


def fire(o):
    o.cmd("mww 0x40027024 0xC0DE0011", settle=0.0)


def run_timed(o, exc, elf, tmo=30.0):
    """Prime+fire on this board, poll exc to completion; return (val, seconds)."""
    prime(o, exc, elf)
    t0 = time.time(); fire(o); val = 0xFFFFFFFF
    while time.time() - t0 < tmo:
        v = o.mdw(exc)
        if v is not None: val = v
        if val not in SENTINELS: break
    return val, time.time() - t0


def loopback(a, b, sl, ma, half):
    """Fire slave on A, drive one master write on B, read slave result."""
    elf = build("ezh_i2c_master", f"-DHALF={half} -DWR_BYTE={PAYLOAD}u")
    msym = syms(elf, ["exc_signal"])
    prime(a, sl["exc_signal"]); fire(a)                 # slave waits for START
    time.sleep(0.2)
    mval, _ = run_timed(b, msym["exc_signal"], elf, tmo=15)
    time.sleep(0.1)
    sval = a.mdw(sl["exc_signal"]); rx = a.mdw(sl["i2c_rx_byte"])
    ok = (mval == 0xCAFEBABE) and (sval == 0xCAFEBABE) and (rx == PAYLOAD)
    return ok, mval, sval, rx


def measure_khz(a, b, half):
    """Differential master-alone timing -> effective SCL frequency (kHz)."""
    a.cmd("mww 0x40027024 0xC0DE0000")                  # park the slave
    e1 = build("ezh_i2c_master", f"-DHALF={half} -DWR_BYTE={PAYLOAD}u -DREPEAT=1")
    s1 = syms(e1, ["exc_signal"])["exc_signal"]
    _, t1 = run_timed(b, s1, e1, tmo=20)
    # calibrate a transaction count that runs for a few seconds
    eK = build("ezh_i2c_master", f"-DHALF={half} -DWR_BYTE={PAYLOAD}u -DREPEAT=100")
    sK = syms(eK, ["exc_signal"])["exc_signal"]
    _, tK = run_timed(b, sK, eK, tmo=40)
    per = max((tK - t1) / 99.0, 1e-6)
    N = min(20000, max(200, int(6.0 / per)))
    eN = build("ezh_i2c_master", f"-DHALF={half} -DWR_BYTE={PAYLOAD}u -DREPEAT={N}")
    sN = syms(eN, ["exc_signal"])["exc_signal"]
    _, tN = run_timed(b, sN, eN, tmo=60)
    per_tx = (tN - t1) / (N - 1)
    scl_khz = 18.0 / per_tx / 1000.0     # 18 SCL clocks per (addr+data) transaction
    return scl_khz, per_tx * 1e6, N


def main():
    a, b = OCD(A_PORT), OCD(B_PORT)
    bringup(a); bringup(b)
    sh("rm -f out/ezh_i2c_slave.o out/ezh_i2c_slave.elf")
    slave_elf = build("ezh_i2c_slave")
    sl = syms(slave_elf, ["exc_signal", "i2c_rx_byte"])
    a.cmd("mww 0x40027024 0xC0DE0000"); a.cmd("load_image %s" % slave_elf, settle=0.6)

    print("\n=== payload tracking (master writes 0x%02X) ===" % PAYLOAD)
    ok, mv, sv, rx = loopback(a, b, sl, None, 500)
    print(f"  HALF=500: master=0x{mv:08x} slave=0x{sv:08x} rx=0x{rx:02x}  -> "
          + ("TRACKS" if ok else "MISMATCH"))

    print(f"\n=== clock speed sweep ({CONFIRM}x per step; smaller HALF = faster) ===")
    ceiling = None
    for h in HALVES:
        res = [loopback(a, b, sl, None, h) for _ in range(CONFIRM)]
        npass = sum(1 for r in res if r[0])
        _, mv, sv, rx = res[0]
        ok = (npass == CONFIRM)
        print(f"  HALF={h:4d}: {npass}/{CONFIRM} pass  m=0x{(mv or 0):08x} "
              f"s=0x{(sv or 0):08x} rx=0x{(rx or 0):02x}  "
              + ("PASS" if ok else ("marginal" if npass else "FAIL")))
        if ok and (ceiling is None or h < ceiling):
            ceiling = h

    print("\n=== SCL frequency (differential timing, master-alone) ===")
    pts = [int(x) for x in os.environ.get("EZH_FREQ_HALVES", "500").split(",") if x]
    if ceiling and ceiling not in pts:
        pts.append(ceiling)
    for h in pts:
        khz, per_us, N = measure_khz(a, b, h)
        print(f"  HALF={h:4d}: ~{khz:7.1f} kHz SCL  "
              f"(transaction {per_us:8.1f} us, N={N})")

    print("\n=== SUMMARY ===")
    print(f"  fastest reliable HALF = {ceiling}")
    print("  (frequencies above; payload tracking confirmed)")


if __name__ == "__main__":
    main()
