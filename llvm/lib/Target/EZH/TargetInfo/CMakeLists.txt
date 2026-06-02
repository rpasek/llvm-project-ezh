#
# Description:
#   CMake build configuration for the LLVMEZHTargetInfo library.
#
# Copied From:
#   Lanai target backend (llvm/lib/Target/Lanai/TargetInfo/CMakeLists.txt).
#
# Changes:
#   Updated library name from LanaiTargetInfo to EZHTargetInfo.

add_llvm_component_library(LLVMEZHInfo
  EZHTargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support

  ADD_TO_COMPONENT
  EZH
  )
