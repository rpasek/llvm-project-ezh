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

TARGET = ezh_test
OUT_DIR = out
LLVM_BUILD_DIR = ../../build
CLANG = $(LLVM_BUILD_DIR)/bin/clang
LD = $(LLVM_BUILD_DIR)/bin/ld.lld
OBJCOPY = $(LLVM_BUILD_DIR)/bin/llvm-objcopy
OBJDUMP = $(LLVM_BUILD_DIR)/bin/llvm-objdump

EZH_TEST_DIR = ..
DEVICES_DIR = $(EZH_TEST_DIR)/third_party/nxp/evkmimxrt595/devices/MIMXRT595S
DRIVERS_DIR = $(DEVICES_DIR)/drivers

CFLAGS = -target ezh-none-elf -g -Os -ffunction-sections -fdata-sections \
         -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS += -isystem $(LLVM_BUILD_DIR)/libc/libc/include
CFLAGS += -I $(EZH_TEST_DIR)
CFLAGS += -I $(EZH_TEST_DIR)/ezh_test
CFLAGS += -I $(EZH_TEST_DIR)/../lldb/source/Plugins/Process/EZH
CFLAGS += -I $(DEVICES_DIR)
CFLAGS += -I $(DEVICES_DIR)/periph
CFLAGS += -I $(DRIVERS_DIR)
CFLAGS += -D__TEST__
CFLAGS += -D__XTENSA__
CFLAGS += -fno-builtin
CFLAGS += -fdwarf-exceptions
CFLAGS += -DCPU_MIMXRT595SFFOC_dsp

SRCS = main.c \
       $(EZH_TEST_DIR)/crt0.c \
       $(EZH_TEST_DIR)/libc_stubs/libc_stubs.c \
       $(DRIVERS_DIR)/fsl_ctimer.c \
       $(DRIVERS_DIR)/fsl_clock.c \
       $(DRIVERS_DIR)/fsl_reset.c \
       $(DRIVERS_DIR)/fsl_inputmux.c

OBJS = $(patsubst %.c, $(OUT_DIR)/%.o, $(notdir $(SRCS)))

# Tell make where to find sources
vpath %.c $(EZH_TEST_DIR) $(EZH_TEST_DIR)/libc_stubs $(DRIVERS_DIR) .

LIBC_A = $(LLVM_BUILD_DIR)/libc/libc/lib/libc_nano.a
BUILTINS_A = \
    $(LLVM_BUILD_DIR)/compiler-rt/lib/linux/libclang_rt.builtins-ezh.a

all: $(OUT_DIR) $(OUT_DIR)/$(TARGET).elf $(OUT_DIR)/$(TARGET).disasm

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

$(OUT_DIR)/%.o: %.c | $(OUT_DIR)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(OUT_DIR)/$(TARGET).elf: $(OBJS)
	$(LD) -T $(EZH_TEST_DIR)/smartdma.ld --gc-sections --discard-locals \
		$(OBJS) $(LIBC_A) $(BUILTINS_A) -Map=$(OUT_DIR)/$(TARGET).map \
		-o $@

$(OUT_DIR)/$(TARGET).disasm: $(OUT_DIR)/$(TARGET).elf
	$(OBJDUMP) -d --triple=ezh $< > $@

clean:
	rm -rf $(OUT_DIR)

.PHONY: all clean
