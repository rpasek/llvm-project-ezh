#
# Description:
#   CMake build configuration for the EZH target backend component group.
#
# Copied From:
#   Lanai target backend (llvm/lib/Target/Lanai/CMakeLists.txt).
#
# Changes:
#   Replaced Lanai component names with EZH; configured TableGen generation
#   targets (register info, instruction info, DAG ISel, calling conventions,
#   asm writer/matcher, disassembler, code emitter, subtarget info) and listed
#   EZH source files.
#
add_llvm_component_group(EZH)
# -Wextra re-enables -Wunused-parameter, which LLVM's own build disables globally
# (append("-Wno-unused-parameter") in HandleLLVMOptions) because it fires on the
# unused args of virtual-method default bodies in generic LLVM headers and on
# TableGen-generated .inc files -- neither of which is EZH code. Restore LLVM's
# policy here so -Werror stays strict for real EZH warnings without policing
# headers/generated code.
add_compile_options(-Werror -Wall -Wextra -Wundef -Wno-unused-parameter)

set(LLVM_TARGET_DEFINITIONS EZH.td)
tablegen(LLVM EZHGenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM EZHGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM EZHGenCallingConv.inc -gen-callingconv)
tablegen(LLVM EZHGenDAGISel.inc -gen-dag-isel)
tablegen(LLVM EZHGenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM EZHGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM EZHGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM EZHGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM EZHGenSDNodeInfo.inc -gen-sd-node-info)
tablegen(LLVM EZHGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(EZHCommonTableGen)

add_llvm_target(EZHCodeGen
  EZHAsmPrinter.cpp
  EZHBasicBlockInfo.cpp
  EZHBitSliceInjection.cpp
  EZHCompareFusion.cpp
  EZHConstantIslandPass.cpp
  EZHFrameLowering.cpp
  EZHInstrInfo.cpp
  EZHISelDAGToDAG.cpp
  EZHISelLowering.cpp
  EZHMachineFunctionInfo.cpp
  EZHMCInstLower.cpp
  EZHRegisterInfo.cpp
  EZHSelectionDAGInfo.cpp
  EZHSubtarget.cpp
  EZHTargetMachine.cpp


  LINK_COMPONENTS
  Analysis
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  EZHDesc
  EZHInfo
  MC
  SelectionDAG
  Support
  Target
  TargetParser
  TransformUtils

  ADD_TO_COMPONENT
  EZH
  )

add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
