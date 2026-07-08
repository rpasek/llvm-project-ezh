#===-- CMakeLists.txt -----------------------------------------------------===#
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===------------------------------------------------------------------------===#
#
# Description:
#   Defines the CMake build target lldbPluginProcessEZH for LLDB's EZH remote
#   process debugging plugin.
#
#===------------------------------------------------------------------------===#

# -Wextra re-enables -Wunused-parameter, which LLVM/LLDB disable globally because
# it fires on generic LLVM/LLDB headers (not EZH code). Restore that policy here
# so -Werror stays strict for real EZH-plugin warnings only. (Matches the EZH
# target directory in llvm/lib/Target/EZH/CMakeLists.txt.)
add_compile_options(-Werror -Wall -Wextra -Wno-unused-parameter)

add_lldb_library(lldbPluginProcessEZH PLUGIN
  ProcessEZH.cpp
  RegisterContextEZH.cpp
  ThreadEZH.cpp

  LINK_LIBS
    lldbCore
    lldbTarget
    lldbPluginProcessGDBRemote
  LINK_COMPONENTS
    Support
  )
