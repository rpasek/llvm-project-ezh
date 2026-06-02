#
# Description:
#   CMake build configuration for the LLVMEZHMCTargetDesc library.
#
# Copied From:
#   Lanai target backend (llvm/lib/Target/Lanai/MCTargetDesc/CMakeLists.txt).
#
# Changes:
#   Updated library name from LanaiMCTargetDesc to EZHMCTargetDesc and listed
#   EZH source files.

#
add_llvm_component_library(LLVMEZHDesc
  EZHAsmBackend.cpp
  EZHELFObjectWriter.cpp
  EZHInstPrinter.cpp
  EZHMCAsmInfo.cpp
  EZHMCCodeEmitter.cpp
  EZHMCTargetDesc.cpp

  LINK_COMPONENTS
  EZHInfo
  MC
  MCDisassembler
  Support
  TargetParser

  ADD_TO_COMPONENT
  EZH
)
