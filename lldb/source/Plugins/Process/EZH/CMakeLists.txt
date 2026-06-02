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

add_compile_options(-Werror -Wall -Wextra)

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
