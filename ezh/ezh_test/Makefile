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

OUT_DIR = out
LLVM_BUILD_DIR = ../../build
CLANG = $(LLVM_BUILD_DIR)/bin/clang
CLANGXX = $(LLVM_BUILD_DIR)/bin/clang++
LD = $(LLVM_BUILD_DIR)/bin/ld.lld
OBJDUMP = $(LLVM_BUILD_DIR)/bin/llvm-objdump

BITSLICE_FLAGS ?=

CFLAGS = -target ezh-none-elf $(BITSLICE_FLAGS) -g -Os \
         -fdwarf-exceptions -ffunction-sections -fdata-sections \
         -Wall -Wextra -Werror -fno-builtin \
         -isystem $(LLVM_BUILD_DIR)/libc/libc/include \
         -I ../../libc -I ../../lldb/source/Plugins/Process/EZH/ -I . -I .. \
         -D__TEST__ -DSTACK_SIZE_WORDS=1280

CXXFLAGS = $(filter-out -g -fdwarf-exceptions, $(CFLAGS)) -fexceptions

LIBC_A = $(LLVM_BUILD_DIR)/libc/libc/lib/libc_nano.a
LIBM_A = $(LLVM_BUILD_DIR)/libc/libc/lib/libm.a
BUILTINS_A = \
    $(LLVM_BUILD_DIR)/compiler-rt/lib/linux/libclang_rt.builtins-ezh.a

C_TESTS = ezh_basic ezh_comp ezh_comp_64 ezh_comp_float ezh_comp_double \
          ezh_arith ezh_other ezh_shift ezh_bit ezh_array \
          ezh_arith_float ezh_arith_double ezh_print ezh_bitslice_test \
          ezh_setjmp
CXX_TESTS = ezh_eh

ALL_TESTS ?= $(C_TESTS) $(CXX_TESTS)
ELFS = $(patsubst %, $(OUT_DIR)/%.elf, $(ALL_TESTS))
DISASMS = $(patsubst %, $(OUT_DIR)/%.disasm, $(ALL_TESTS))

all: $(OUT_DIR) $(ELFS) $(DISASMS)

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

$(OUT_DIR)/crt0.o: ../crt0.c | $(OUT_DIR)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(OUT_DIR)/libc_stubs.o: ../libc_stubs/libc_stubs.c | $(OUT_DIR)
	$(CLANG) $(CFLAGS) -c $< -o $@

# C Test Rules
$(OUT_DIR)/%.o: %.c ezh_test.h | $(OUT_DIR)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(OUT_DIR)/%.elf: $(OUT_DIR)/%.o $(OUT_DIR)/crt0.o $(OUT_DIR)/libc_stubs.o
	$(LD) -T ../smartdma.ld --gc-sections --discard-locals \
		$(OUT_DIR)/crt0.o $(OUT_DIR)/libc_stubs.o $< $(LIBC_A) $(BUILTINS_A) \
		-Map=$(OUT_DIR)/$*.map -o $@

# C++ Test Rules
$(OUT_DIR)/ezh_eh.o: ezh_eh.cpp ezh_test.h | $(OUT_DIR)
	$(CLANGXX) $(CXXFLAGS) -c $< -o $@

$(OUT_DIR)/ezh_eh.elf: $(OUT_DIR)/ezh_eh.o $(OUT_DIR)/crt0.o $(OUT_DIR)/libc_stubs.o
	$(LD) -T ../llvm_test/smartdma_large.ld --gc-sections --discard-locals \
		$(OUT_DIR)/crt0.o $(OUT_DIR)/libc_stubs.o $< \
		-L$(LLVM_BUILD_DIR)/libc/lib -L$(LLVM_BUILD_DIR)/libc/libc/lib \
		$(BUILTINS_A) $(LIBC_A) $(LIBM_A) -lc++ -lc++abi -lunwind \
		-Map=$(OUT_DIR)/ezh_eh.map -o $@

$(OUT_DIR)/%.disasm: $(OUT_DIR)/%.elf
	$(OBJDUMP) -d --triple=ezh $< > $@

run: all
	@echo "=== Running all tests ==="
	@export EZH_TEST_EXECUTE_DIR="$$(pwd)/$(OUT_DIR)" && \
	$(LLVM_BUILD_DIR)/bin/lldb -b \
		-o "command script import ezh_test_runner.py"

clean:
	rm -rf $(OUT_DIR)

.PHONY: all run clean
