# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#===----------------------------------------------------------------------===#
# EZH Baremetal CMake Toolchain File
#
# This file configures the compiler settings and system target parameters
# for the ezh-none-elf baremetal target, bypassing all host compile tests
# and surgically bypassing EZH-incompatible target profiling tools.
#===----------------------------------------------------------------------===#

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ezh)

# Force CMake to skip all compiler checks and sanity compiles
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

# Define basic architecture properties
set(CMAKE_SIZEOF_VOID_P 4)
set(WORDS_BIGENDIAN OFF)
set(CMAKE_C_BYTE_ORDER LITTLE_ENDIAN)

# Explicitly notify CMake that we support static libraries only
set(TARGET_SUPPORTS_SHARED_LIBS FALSE)
