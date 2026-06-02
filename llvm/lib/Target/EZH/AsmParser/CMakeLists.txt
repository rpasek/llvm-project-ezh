#
# Description:
#   CMake build configuration for the LLVMEZHAsmParser library.
#
# Copied From:
#   Lanai target backend (llvm/lib/Target/Lanai/AsmParser/CMakeLists.txt).
#
# Changes:
#   Updated library name from LanaiAsmParser to EZHAsmParser and listed EZH
#   source files.

#
include_directories( ${CMAKE_CURRENT_BINARY_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/.. )

add_llvm_component_library(LLVMEZHAsmParser
  EZHAsmParser.cpp

  LINK_COMPONENTS
  CodeGenTypes
  EZHDesc
  EZHInfo
  MC
  MCParser
  Support

  ADD_TO_COMPONENT
  EZH
  )

add_dependencies( LLVMEZHAsmParser EZHCommonTableGen )
