# EZH (SmartDMA) LLDB Debugger Plugin & Target Architecture

This directory contains the LLDB process plugin (`ezh-remote`) and register
context implementation for debugging NXP SmartDMA (EZH) processor cores over
JTAG/SWD connections (e.g., via OpenOCD).

Because the EZH processor architecture lacks traditional hardware debug modules
(such as ARM CoreSight debug units or hardware single-stepping) and provides
only a single, hazardous hardware breakpoint register, debugging on EZH relies
on a **cooperative software-virtualized debugging model** between target
runtime firmware (`crt0.c`) and this LLDB plugin.

---

## 1. Architectural Overview & Hardware Constraints

When designing a debugger for EZH, several fundamental hardware characteristics
and limitations dictate the architecture:

* **Limited/Hazardous Hardware Breakpoints**: While a single hardware breakpoint
  register exists (`EZHB_BREAK_ADDR_OFFSET`), relying on it is severely limiting
  for debugging workflows. More critically, there is a hazard when placing the
  hardware breakpoint 1 or 2 instructions after a PC-relative load
  (`ldr r[n], pc, [offset]`); doing so causes the load instruction to read from
  the wrong address and return incorrect data.
* **No Hardware Halt Facilities**: There are no hardware control bits that a
  debugger can assert to directly freeze the CPU in hardware.
* **No Hardware Single-Step Unit**: The core cannot single-step instruction
  cycles natively in hardware.
* **No Hardware Register Inspection Facilities**: There are no facilities to
  inspect or modify general-purpose CPU registers using debug hardware.
  Furthermore, while read-only registers exist on the AHB bus for `EZHB_SP` and
  `EZHB_PC`, hardware reads from these registers return incorrect values.

To overcome this complete lack of hardware debug inspection and control units,
EZH implements a **virtual debug monitor** in software, coordinated with LLDB
over standard memory read/write operations via a GDB server such as OpenOCD.

## 2. Summary of Core Debug Mechanisms
To debug the EZH core cooperatively over standard memory read/write operations,
the architecture relies on three software-virtualized mechanisms:
* **Halting**: Because EZH hardware lacks an interrupt controller, interrupts
  are emulated in software via the `-mattr=+bitslice-interrupts` compiler
  feature, which injects a conditional `gotol_bs __ezh_bitslice_handler` branch
  before every `goto` or `gosub` instruction. To halt a running target
  asynchronously (such as during Ctrl-C), LLDB asserts the AHB accessible
  (`EZHB_PENDTRAP`) register, setting bitslice 7 to raise the CPU's internal
  `BS` (BitSlice) condition code flag. When the CPU executes the next injected
  `gotol_bs` instruction, a branch is taken into `__ezh_bitslice_handler`,
  which jumps into the debug monitor and freezes in an idle spin loop. Because
  a `gotol` (goto with link) is used, the current PC is now in RA, allowing us
  to know the PC at the point of interruption.
* **Software Breakpoints**: LLDB injects a `goto` instruction in place of the
  user instruction in RAM, which jumps to a dedicated breakpoint slot
  containing return address (RA) preservation code `str sp, ra, -8` followed by
  `gosub __ezh_debug_common`. When reached, the CPU jumps to the slot, saves its
  return address to the stack and enters the debug monitor and freezes in an
  idle spin loop. Since LLDB knows where it injected the `goto` instruction and
  each breakpoint slot is unique, each `gosub __ezh_debug_common` will generate
  a unique return address in RA, allowing lldb to deduce the PC at the point of
  the breakpoint.
* **Register Inspection/Modification**: Because hardware debug registers are
  absent or broken, the debug monitor (`__ezh_debug_common`) saves all 16 CPU
  registers and 1 virtual flag register into a 68-byte memory dump on the
  target's stack RAM and publishes the frame pointer to a global variable
  (`__ezh_debug_frame`). LLDB inspects and modifies CPU registers by reading
  and writing this RAM structure.

---

## 3. Target Runtime Architecture

The target runtime provides two exported global symbols defined in
`EZHRegisters.h` and implemented in `crt0.c` that form the bridge between
running code and LLDB:

```c
void __ezh_debug_common(void);
extern volatile uint32_t __ezh_debug_frame;
```

### `__ezh_debug_common`
This naked assembly function serves as the universal entry point for all debug
halts. It is essentially the virtual debug monitor.

Upon entry to `__ezh_debug_common`:
1. **68-Byte Register Dump**: It saves all 17 CPU registers onto the target's
   stack in a fixed layout (`r0-r7`, `gpo`, `gpd`, `cfs`, `cfm`, `sp`, `pc`,
   `gpi`, `ra`, `flags`). Note that `flags` is a virtual register synthesized
   by the debugger framework. EZH has no physical register for ALU status flags.
   The runtime performs conditional execution to populate a 32-bit word,
   allowing LLDB to display and restore condition codes upon resume.
2. **Halt Notification**: It stores the resulting stack frame base pointer
   (`sp`) into the volatile RAM variable `__ezh_debug_frame`.
3. **Idle Spin Loop**: It enters a tight idle loop, continuously reading
   `__ezh_debug_frame` from RAM. The target remains frozen in this virtual halt
   until LLDB resets `__ezh_debug_frame` to `0` via an AHB memory write over
   JTAG.
4. **Clean Restoration**: Once released by LLDB, it pops the 68-byte frame from
   the stack, restoring all GPRs, special registers, and ALU flags to their
   pre-halt state, and branches back to user execution. ALU flags are restored
   by performing an ALU operations that recreate the original flags.

### DWARF CFI & Stack Unwinding
To ensure external debuggers can inspect local variables and unwind call stacks
while stopped inside interrupt handlers or software traps, `crt0.c` is compiled
with `-fdwarf-exceptions`. Manual CFI directives (`.cfi_def_cfa_offset`,
`.cfi_offset`, `.cfi_restore`) track stack pointer modifications and register
saves across both function prologues and epilogues.

### Mandatory ELF Symbol Dependency
Because EZH has no hardware debug registers to interrogate CPU state or locate
monitors in memory, **debugging capabilities cannot function without an active
ELF file loaded into LLDB**. Without a symbol-bearing ELF, LLDB does not know
where—or even if—the essential runtime symbols (`__ezh_debug_common` and
`__ezh_debug_frame`) are present in memory. Consequently, while attaching to
bare silicon without an executable symbol file will succeed, none of the debug
capabilities (register inspection, software breakpoints, stepping, or halting)
will work without resolving those symbol addresses from the ELF symbol table.

### Stack Pointer Initialization Dependency
Because `__ezh_debug_common` relies on the target's stack to dump and restore
the 68-byte register frame, **debugging cannot function until the stack pointer
(`sp`) has been initialized by target firmware**. If a breakpoint or halt trap
occurs before startup code (`crt0.c`) initializes `sp`, the monitor will
attempt to push registers onto an invalid or zero stack address, resulting in
memory corruption or a fatal CPU fault. This is the reason why **you must
use `continue` instead of `step` before the core is ignited**; stepping from
reset will immediately attempt to trap into the monitor on an uninitialized
stack, whereas continuing allows firmware to execute cleanly through startup
until `sp` is properly established.

---

## 4. LLDB Plugin Architecture (`ProcessEZH`)

`ProcessEZH` subclasses `lldb_private::process_gdb_remote::ProcessGDBRemote` to
communicate with standard OpenOCD servers while overriding breakpoint,
stepping, register, and connection behaviors.

### Bypassing OS & Symbol Queries (`DidAttach` / `WillPublicStop`)
When attaching to the target, standard LLDB sends OS, dynamic linker, and
symbol lookup queries (`qProcessInfo`, `qSymbol::`, structured data queries) to
OpenOCD, which we do not support.
* `ProcessEZH::DidAttach` and `WillPublicStop` are overridden as no-ops to
  bypass these queries.

### Register Context (`RegisterContextEZH`)
Because there are no hardware facilities to inspect CPU registers,
`RegisterContextEZH` completely avoids querying hardware registers for thread
states:
* When halting, LLDB reads the memory address stored in `__ezh_debug_frame`.
* Using this base pointer, LLDB reads and writes the 68-byte register dump
  directly from/to target stack RAM.
* DWARF register numbers (0–16) map 1-to-1 with LLDB register indices, enabling
  zero-overhead CFI translation.

---

## 5. Software Breakpoint Management

Because of the hardware 1-breakpoint limit, hazard associated with the hardware
breakpoint and EZH not supporting standard GDB-remote breakpoint packets
(`Z0` / `z0`), `ProcessEZH::UpdateBreakpointSites` delegates directly to the
base class `Process::UpdateBreakpointSites`, bypassing `ProcessGDBRemote`.
LLDB manages instruction memory patching directly via `EnableBreakpointSite`
and `DisableBreakpointSite`.

### Goto Injection & The 8-Byte Preservation Slot
To trap execution without corrupting user registers, an EZH software breakpoint
consists of two parts:
1. **In-Place Goto Injection**: At the target user instruction address, LLDB
   replaces the original 4-byte opcode with a `goto slot_addr` instruction
   pointing to an available breakpoint slot.
2. **RA Preservation Code (in Slot)**: In the allocated slot (`slot_addr`),
   LLDB writes an 8-byte sequence:
   * `str sp, ra, -8`: Saves the current return address (`ra`) to stack
     offset `-8`.
   * `gosub __ezh_debug_common`: Subroutine jump to the virtual debug handler
     (which overwrites `ra` with the return address, allowing the handler to
     calculate the exact address of the breakpoint slot).

### Breakpoint Capacity & Slot Management
* **Linker-Script Derived Capacity**: There is no hardcoded breakpoint limit in
  LLDB. When a module loads, `ProcessEZH` scans for memory sections named
  `.text.ezh_breakpoints*`. The number of available breakpoint slots in each
  region is dynamically calculated as the section size in bytes divided by 8
  (`region.size / 8`), meaning total capacity is controlled entirely by how
  large the slot regions are defined in the target linker script.
* **Goto Branch Range & Multiple Region Support**: In the EZH instruction set,
  direct unconditional jumps (`goto`) encode a limited **21-bit page-relative
  branch target** (`EZH_PAGE_BASE_MASK_21BIT`). An injected `goto slot_addr`
  can only reach a slot that resides within the same 21-bit memory page as the
  instruction being replaced. For programs spanning multiple code pages or
  memory banks, a single centralized breakpoint slot section would be
  unreachable from distant instructions. To solve this, the linker script can
  define multiple localized slot regions (`.text.ezh_breakpoints*` across
  different sections). When setting a breakpoint at `addr`, LLDB iterates
  through `m_breakpoint_regions` and selects a slot from a region on the same
  page (`IsOnSamePage(addr, region.start_addr)`), ensuring branch reachability.
* When a breakpoint is hit, `ProcessEZH` resolves the sentinel PC back to the
  original user instruction address.
* When stepping or continuing over an active breakpoint site, LLDB temporarily
  restores the original 4-byte instruction opcode (replacing the injected
  `goto`), executes a single step, and re-applies the `goto slot_addr`
  instruction.

---

## 6. Instruction-Level Single Stepping (`DoResume`)

Because EZH lacks hardware single-stepping, `ProcessEZH` implements instruction
step (`si` / `next`) via dynamic PC prediction:
1. Before resuming from a step command, `ProcessEZH::PredictPCDestination`
   disassembles the instruction at the current PC.
2. It evaluates conditional branch opcodes (`goto`, `goto_reg`, `gosub`,
   conditional skips) against current CPU flags read from the register context.
3. By evaluating branch conditions against the active ALU flags from the RAM
   frame, it predicts the target's exact next execution address (taken or
   not-taken) and inserts a single temporary software breakpoint (`goto`
   injection) at that destination.
4. It sets `__ezh_debug_frame = 0` and resumes execution. When the target hits
   the predicted breakpoint, it halts, completing the step cycle.

---
