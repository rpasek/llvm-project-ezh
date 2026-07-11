# LLVM EZH

LLVM toolchain for NXP's EZH (SmartDMA) architecture

EZH is an extra core integrated into many NXP MCUs. It's similar to a Cortex M0 in performance and design. It contains no hardware multiply or divide but it can perform an ALU operation, a shift, set a condition code and conditionally execute on nearly every instruction.

Crucially EZH does not have support for real interrupts. It does have support for setting the bs (bitslice) flag whenever an external interrupt is set. LLVM EZH takes advantage of this by adding an additional `gotol_bs bitslice_handler` before every goto and gosub instruction.  This essentially allows for interrupt capability with non deterministic interrupt latency. This functionality can be disabled by passing `-mno-ezh-bitslice-interrupts` to clang.

Please see
* [AN14650: SmartDMA Cookbook](https://www.nxp.com/docs/en/application-note/AN14650.pdf) for details on the architecture.
* [fsl_smartdma_prv.h](https://github.com/nxp-mcuxpresso/mcuxsdk-core/blob/main/drivers/smartdma/fsl_smartdma_prv.h) for the full instruction set

## Notice

LLVM EZH is not officially affiliated with or endorsed by the LLVM Foundation or LLVM project.

This fork provides a complete toolchain for NXP's EZH (SmartDMA) architecture. and aims for a possible integration with upstream LLVM in the future. Target non specific changes are kept to an absolute minimum. Key issues need to be addressed for this work to be considered on upstream LLVM:
1. EZH's development community is currently too small and doesn't meet the requirements of [LLVM Developer Policy: Adding a New Target](https://llvm.org/docs/DeveloperPolicy.html#adding-a-new-target)
2. This target is vibecoded and doesn't currently meet the requirements of [LLVM AI Tool Use Policy](https://llvm.org/docs/AIToolPolicy.html). A human was not in the loop during all aspects of development. While the code has been extensively reviewed and many corrections have been made, more review is still required.
3. The commits need to be broken down into small logical steps that are possible to be reviewed.

This toolchain is for development only. No binaries are provided.

While this toolchain passes all of the llvm-test-suite/SingleSource/Regression tests (this includes the GCC torture tests) at O0 and Os optimization levels, the output should heavily scrutinized and not used in safety critical systems.

# Building LLVM EZH

Compiling LLVM EZH follows the same convention as compiling LLVM.

First, please review the [hardware and software requirements](https://llvm.org/docs/GettingStarted.html#requirements) for building LLVM.

## Clone the LLVM EZH repository

```
git clone https://github.com/rpasek/llvm-project-ezh.git
```

## Build the LLVM EZH project

Helper scripts are provided to build the toolchain and run target tests.x

These scripts require Python and Ninja to be installed.

* **To build only the LLVM EZH toolchain and runtimes:**
  ```bash
  ./build_llvm.sh
  ```

* **To build the toolchain and run `ezh_test` regression suite on target hardware:**
  ```bash
  ./build_and_test.sh
  ```

Note: Target tests requires OpenOCD (or another GDB server) to be running on localhost:3333.

## Testing LLVM EZH

An extensive amount of tests are provided in the ezh folder. All tests have been developed to on the RT595. They will likely run on other cores with small changes to the linker scripts.

The tests:
* llvm_test: llvm-test-suite/SingleSource/Regression test suite (this includes the GCC torture test suite) with llvm-lit with -O0 and -Os optimizations. This test uses a smartdma_large.ld linkerscript that will wipe out M33 memory so it's recommend having the M33 halted when running this code. The rest of the tests use the reserved 32K of SmartDMA RAM.
* ezh_test: This is a home grown test suite (with some EZH specific tests) that combines many tests into a few ELFs allowing for testing many things in minimal time. This test is run every time build_and_test.sh is run to help guarantee that changes made to LLVM still result in baseline functionality.
* csmith_test: This uses csmith (a random c code generator) to generate some pretty terrible looking code that pushes the limits of compilers. You will need to apt install this to use it.
* ctimer_test: This configures CTIMER2 to generate an interrupt on EZH every second and expects 5 interrupts for the code to pass. EZH is busy doing arbitrary work during this time, which helps guarantee that EZH can handle an interrupt and return without corrupting the interrupted state. It's worth pointing out that EZH doesn't support interrupts. This test is specifically crafted to prove that emulated interrupts (otherwise referred to as bitslice interrupts) work correctly.
* debug_test: This tests lldb functionality. It guarantees that halting, continuing, stepping and variable inspection work correctly. It's worth mentioning that debugging is implemented entirely in software. Breakpoints work by lldb swapping instructions out with `goto debug_software_breakpoint_[n]`. Stepping works simply by moving the breakpoint to the next instruction or next logical line. Halting works by triggering a bitslice interrupt on vector 7 (jumps to the debug handler) which is now reserved for debugging.

# Help us out

Feel free to contribute! PRs are welcome!

All LLVM EZH code should observe the [LLVM coding standards](https://llvm.org/docs/CodingStandards.html).

Code should be appropriately documented and well tested.

While this target is entirely AI generated please keep in mind a long term goal of this project is to land this code in upstream LLVM as an experimental target. Please follow [LLVM AI Tool Use Policy](https://llvm.org/docs/AIToolPolicy.html) for any AI generated contributions.
