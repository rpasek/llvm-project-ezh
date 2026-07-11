We are building a production LLVM port for a NXP developed processor
architecture called EZH, otherwise known as SmartDMA. There is very little
information about EZH anywhere on the internet. The only published information
on EZH is located in 2 files located this folder. They are:

* AN14650.pdf: Detailed information on how the processor functions. This file
  has been converted to a text file called AN14650.txt
* fsl_smartdma_prv.h: Defines used for constructing machine code in a C array.
  Some modifications have been made to this file based on experiments we have
  made to this file. We have built a test into LLVM to ensure that all of our
  changes adhere to fsl_smartdma_prv.h. This test is located at
  llvm/test/MC/EZH/instructions.s. Whenever we change anything
  related to the instruction encoding, we should run this test to make sure we
  didn't break anything.

I reiterate, this is the only information available for this core.

Our LLVM target is located at llvm/lib/Target/EZH/

The EZH core we are experimenting with is located in the NXP RT595. We mostly
test functionality with tests in ezh/ezh_test folder. We can run test on a real
silicon EZH core with ./run.sh inside that folder.

Because of the limited information present to us in the documentation, we should
expect to have to do significant amounts of experimentation. We typically write
inline assembly in ezh/ezh_test/ezh_basic.c and remove our changes after we get
the answers we are looking for.

To better communicate our experimental results, we discuss without emotions, in
a calm level headed tone.

This core has no cache and has had extensive testing. All instructions have been
tested and proven to work. We should approach writing code with the idea that it
has no bugs or errata.

This core is somewhat similar to ARM. We use PC relative loads for constants and
we have a +508/-512 offset range with E_LDR instructions. Because of this, we've
largely copied the ARM  Constant Island technique and we should look to copy it
as much as possible going forward. Significant work has been done on the
Constant Island code and is likely still very buggy.

A couple of important notes:
* This core has no overflow flag in the ALU and this makes 32 and 64 bit signed
  comparisons more complicated
* The ALU flags operate more similarly to x86 (less like ARM). For example the
  CA flag is raised when there is an unsigned overflow or borrow out of the most
  significant bit.
* This core can only do 8 or 32 bit loads and stores.
* The instruction format and register ordering used in the assembler matches
  as shown in fsl_smartdma_prv.h. The most major exception is condition codes
  are given in the assembler proceeding the instruction. For example e_goto_ca.
  Please pay careful attention to the order of registers here. Many other Gemini
  sessions have gotten confused here.

Every effort should be made to make code of the highest possible quality. We
intend to upstream this work and I expect it to receive a high level of
scrutiny.

LLVM is built and tested with `./build_and_test.sh` inside the `llvm-project`
directory. Always build and test changes using `./build_and_test.sh`.

We don't want to pollute our project. Please do all experimentation in the
scratch folder.

AHB accessible EZHB_SP and EZHB_PC registers are read-only (in hardware).
They are broken and read incorrect values (in hardware).

Reset resets the core, memory is unaffected. If reset is applied, AHB registers
ignore all accesses.