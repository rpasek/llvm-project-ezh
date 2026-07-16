# I3C SDR read: is RXCOUNT current when COMPLETE reads high?

## The question

A SmartDMA master polling I3C during an SDR read wants to trust this:

```c
flags   = base->MSTATUS;
rxCount = (base->MDATACTRL & RXCOUNT_MASK) >> RXCOUNT_SHIFT;
if (flags & kI3C_MasterCompleteFlag) {
    /* exactly rxCount bytes remain on the wire -- safe to drain and stop */
}
```

The worry is a clock-domain skew: the I3C time-control/CCC engine runs off
the slow clock while the bus interface runs off FCLK, so in principle
`COMPLETE` could become observable a beat before the final received byte's
`RXCOUNT` increment crosses into the register a polling master reads. If it
can, a short read (target ends early) that exits on `rxCount == 0 &&
COMPLETE` could drop the last byte.

## The experiment (`run_i3c_readprobe.sh`)

Two EVK-MIMXRT595 boards, master and target, on the J18 I3C header. After
ENTDAA assigns the target 0x30, the master issues many back-to-back
N-byte SDR reads (`MCTRL.DIR=1`, `RDTERM=N`, N <= the 8-deep RX FIFO so
nothing is drained mid-read). The target keeps its TX FIFO topped up with
a rolling pattern so every read delivers N bytes.

For each read the master tight-polls `(MDATACTRL, MSTATUS, MDATACTRL)`
back-to-back and records, at the **first** sample where `COMPLETE` is
high, the `RXCOUNT` value -- both as read *after* the status (the exact
order the master's real code uses) and *before* it (a strictly wider
observation window). It counts any read where that first-COMPLETE
`RXCOUNT` is less than the settled final count. The EZH polls at ~5-10 ns
per sample, far tighter than the byte rate, so the capture straddles the
completion transition.

## Result

Across three read sizes, 4000 reads each (12000 total), at FCLK 24 MHz /
PP 12 MHz / OD 2.4 MHz / slow clock 1 MHz:

| N | reads | short | RXCOUNT at first COMPLETE | skew (engineer order) | skew (wider window) |
|---|-------|-------|---------------------------|-----------------------|---------------------|
| 1 | 4000  | 0     | 1 every time              | 0                     | 0                   |
| 4 | 4000  | 0     | 4 every time              | 0                     | 0                   |
| 8 | 4000  | 0     | 8 every time              | 0                     | 0                   |

Every single time `COMPLETE` first read high, `RXCOUNT` already reflected
all N received bytes -- including the single-byte read (the short-read
worst case) and even when the count was sampled a beat before the status
read. **No skew was observed.**

## Interpretation

On this silicon and clock configuration, reading `MSTATUS` then
`MDATACTRL` and trusting `rxCount` when `COMPLETE` is set is safe: the
`if (COMPLETE) { exactly rxCount bytes remain }` assumption holds.

Honest caveats: this is absence-of-observed-skew (strong evidence), not a
proof of a hardware guarantee -- NXP can confirm whether it is guaranteed
by design or merely holds at these clock ratios. It was measured only at
the config above; the FIFO-count and COMPLETE both come from the same bus
FSM, so a ratio-dependent skew is unlikely, but only NXP can say it is
architecturally impossible.

Note also that for a **known-length** read the master's clean exit is
`bytes_remaining == 0` (all expected bytes drained), which never consults
this timing at all. The `COMPLETE`-based exit is load-bearing only for
target-shortened reads -- exactly the case N=1 above stresses, and it was
clean.
