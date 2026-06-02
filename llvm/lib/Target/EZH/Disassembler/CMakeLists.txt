#
# Description:
#   CMake build configuration for the LLVMEZHDisassembler library.
#
# Copied From:
#   Lanai target backend (llvm/lib/Target/Lanai/Disassembler/CMakeLists.txt).
#
# Changes:
#   Updated library name from LanaiDisassembler to EZHDisassembler and listed
#   EZH source files.

#
add_llvm_component_library(LLVMEZHDisassembler
  EZHDisassembler.cpp

  LINK_COMPONENTS
  CodeGenTypes
  EZHDesc
  EZHInfo
  MC
  MCDisassembler
  Support

  ADD_TO_COMPONENT
  EZH
  )
