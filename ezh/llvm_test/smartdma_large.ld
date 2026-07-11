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

/* WARNING: This linkerscript uses memory regions mapped to the main SRAM.    */
/* This linkerscript CLOBBERS Cortex-M33 memory in RT595!                     */

MEMORY
{
  SMARTDMA (rwx) : ORIGIN = 0x24100000, LENGTH = 32K
  RAM (rwx)      : ORIGIN = 0x00100000, LENGTH = 4096K
}

ENTRY(_start)

SECTIONS
{
  .text.start : {
    KEEP(*(*API_TABLE))
    KEEP(*(.text.bitslice_interrupts*))
    *(.start)
  } > SMARTDMA

  .text.ezh_breakpoints.smartdma : ALIGN(4) {
    . = . + 128;
  } > SMARTDMA

  .exc_signal : {
    KEEP(*(.exc_signal))
  } > SMARTDMA

  .text : {
    *(.text*)
  } > RAM

  .text.ezh_breakpoints.ram : ALIGN(4) {
    . = . + 128;
  } > RAM

  .rodata : {
    *(.rodata*)
    *(.data.rel.ro*)
  } > RAM

  .init_array : {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
    KEEP (*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .ctors))
    PROVIDE_HIDDEN (__init_array_end = .);
  } > RAM

  .fini_array : {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
    KEEP (*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .dtors))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } > RAM

  .data : ALIGN(4) {
    __data_start = .;
    *(.data*)
    . = ALIGN(4);
    __data_end = .;
  } > RAM

  __data_load_start = LOADADDR(.data);

  .bss (NOLOAD) : ALIGN(4) {
    __bss_start = .;
    /*KEEP(*(*printf_buffer)) */
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    __bss_end = .;
  } > RAM

  .stack (NOLOAD) : ALIGN(8) {
    KEEP(*(.stack))
  } > RAM

  .heap (NOLOAD) : ALIGN(4) {
    _end = .;
    . = . + 0x41000;
    __llvm_libc_heap_limit = .;
  } > RAM

  .near_text : {
    KEEP(*(.near_text))
  } > SMARTDMA

  .post_text : {
    KEEP(*(.post_text))
  } > RAM




}
