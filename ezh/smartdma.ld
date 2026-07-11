/*
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

MEMORY
{
  SMARTDMA (rwx) : ORIGIN = 0x24100000, LENGTH = 32K
}

ENTRY(_start)

SECTIONS
{
  .text : {
    KEEP(*(*API_TABLE))
    *(.start)
    KEEP(*(.text.bitslice_interrupts*))
    *(.text*)
  } > SMARTDMA

  .text.ezh_breakpoints.smartdma : ALIGN(4) {
    . = . + 128;
  } > SMARTDMA

  .rodata : {
    *(.rodata*)
    *(.data.rel.ro*)
  } > SMARTDMA

  .data : ALIGN(4) {
    __data_start = .;
    KEEP(*(.exc_signal))
    *(.data*)
    . = ALIGN(4);
    __data_end = .;
  } > SMARTDMA

  __data_load_start = LOADADDR(.data);

  .bss (NOLOAD) : ALIGN(4) {
    __bss_start = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    __bss_end = .;
  } > SMARTDMA

  .stack (NOLOAD) : ALIGN(4) {
    KEEP(*(.stack))
  } > SMARTDMA

  .heap (NOLOAD) : ALIGN(4) {
    _end = .;
    . = . + 0x1000;
    __llvm_libc_heap_limit = .;
  } > SMARTDMA
}