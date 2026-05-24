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

#ifndef EZH_H
#define EZH_H

#include <stdint.h>

#define TO_PER_IMM(addr) addr - 0x40000000

#ifndef EZH_BASE
#define EZH_BASE 0x40027000
#endif

#define EZHB_BOOT_OFFSET 0x20
#define EZHB_CTRL_OFFSET 0x24
#define EZHB_PC_OFFSET 0x28
#define EZHB_SP_OFFSET 0x2C
#define EZHB_BREAK_ADDR_OFFSET 0x30
#define EZHB_BREAK_VECT_OFFSET 0x34
#define EZHB_EMER_VECT_OFFSET 0x38
#define EZHB_EMER_SEL_OFFSET 0x3C
#define EZHB_ARM2EZH_OFFSET 0x40
#define EZHB_EZH2ARM_OFFSET 0x44
#define EZHB_PENDTRAP_OFFSET 0x48

#define EZHB_BOOT (EZH_BASE + EZHB_BOOT_OFFSET)
#define EZHB_CTRL (EZH_BASE + EZHB_CTRL_OFFSET)
#define EZHB_PC (EZH_BASE + EZHB_PC_OFFSET)
#define EZHB_SP (EZH_BASE + EZHB_SP_OFFSET)
#define EZHB_BREAK_ADDR (EZH_BASE + EZHB_BREAK_ADDR_OFFSET)
#define EZHB_BREAK_VECT (EZH_BASE + EZHB_BREAK_VECT_OFFSET)
#define EZHB_EMER_VECT (EZH_BASE + EZHB_EMER_VECT_OFFSET)
#define EZHB_EMER_SEL (EZH_BASE + EZHB_EMER_SEL_OFFSET)
#define EZHB_ARM2EZH (EZH_BASE + EZHB_ARM2EZH_OFFSET)
#define EZHB_EZH2ARM (EZH_BASE + EZHB_EZH2ARM_OFFSET)
#define EZHB_PENDTRAP (EZH_BASE + EZHB_PENDTRAP_OFFSET)

typedef struct {
  uint8_t RESERVED_0[32];
  volatile uint32_t BOOTADR;    /* 0x20 */
  volatile uint32_t CTRL;       /* 0x24 */
  volatile uint32_t _PC;        /* 0x28 */
  volatile uint32_t _SP;        /* 0x2C */
  volatile uint32_t BREAK_ADDR; /* 0x30 */
  volatile uint32_t BREAK_VECT; /* 0x34 */
  volatile uint32_t EMER_VECT;  /* 0x38 */
  volatile uint32_t EMER_SEL;   /* 0x3C */
  volatile uint32_t ARM2EZH;    /* 0x40 */
  volatile uint32_t EZH2ARM;    /* 0x44 */
  volatile uint32_t PENDTRAP;   /* 0x48 */
} EZH_Type;

#define EZH ((volatile EZH_Type *)EZH_BASE)

#define EZH_INPUT_SOURCE_0 0
#define EZH_INPUT_SOURCE_1 1
#define EZH_INPUT_SOURCE_2 2
#define EZH_INPUT_SOURCE_3 3
#define EZH_INPUT_SOURCE_4 4
#define EZH_INPUT_SOURCE_5 5
#define EZH_INPUT_SOURCE_6 6
#define EZH_INPUT_SOURCE_7 7

#define BS0(c) (c << 8)
#define BS1(c) (c << 11)
#define BS2(c) (c << 14)
#define BS3(c) (c << 17)
#define BS4(c) (c << 20)
#define BS5(c) (c << 23)
#define BS6(c) (c << 26)
#define BS7(c) (c << 29)

#define EZH_DISABLE_EMERGENCY_BIT 8

#define EZH_HANDSHAKE_EVENT 0
#define EZH_HANDSHAKE_ENABLE 1

#define EZH_MASK_RESP 2
#define EZH_ENABLE_AHBBUF 3
#define EZH_ENABLE_GPISYNCH 4

/* Bit Slice Mux cfg */

#define BS_1 0
#define BS_RISE 1
#define BS_FALL 2
#define BS_CHANGE 3
#define BS_SIG 4
#define BS_SIGN 5
#define BS_0 6
#define BS_EVENT 7

// === GPO (General-purpose outputs) ===
static inline uint32_t ezh_read_gpo(void) {
  uint32_t val;
  __asm__ volatile("mov %0, gpo\n\t" : "=r"(val));
  return val;
}

static inline void ezh_write_gpo(uint32_t val) {
  __asm__ volatile("mov gpo, %0\n\t" : : "r"(val));
}

// === GPI (General-purpose inputs) ===
static inline uint32_t ezh_read_gpi(void) {
  uint32_t val;
  __asm__ volatile("mov %0, gpi\n\t" : "=r"(val));
  return val;
}

// === GPD (General-purpose directions) ===
static inline uint32_t ezh_read_gpd(void) {
  uint32_t val;
  __asm__ volatile("mov %0, gpd\n\t" : "=r"(val));
  return val;
}

static inline void ezh_write_gpd(uint32_t val) {
  __asm__ volatile("mov gpd, %0\n\t" : : "r"(val));
}

// === CFS (Configuration and Status Selection) ===
static inline uint32_t ezh_read_cfs(void) {
  uint32_t val;
  __asm__ volatile("mov %0, cfs\n\t" : "=r"(val));
  return val;
}

static inline void ezh_write_cfs(uint32_t val) {
  __asm__ volatile("mov cfs, %0\n\t" : : "r"(val));
}

// === CFM (Configuration and Mode) ===
static inline uint32_t ezh_read_cfm(void) {
  uint32_t val;
  __asm__ volatile("mov %0, cfm\n\t" : "=r"(val));
  return val;
}

static inline void ezh_write_cfm(uint32_t val) {
  __asm__ volatile("mov cfm, %0\n\t" : : "r"(val));
}

static inline void ezh_hold(void) { __asm__ volatile("hold\n\t"); }

static inline void ezh_int_trigger(const uint32_t channel) {
  __asm__ volatile("int_trigger %0\n\t" : : "i"(channel));
}

#define API_ENTRY(n)                                                           \
  void __attribute__((naked)) API_ENTRY_##n() {                                \
    __asm__ volatile("nop\n\t"                                                 \
                     "nop\n\t"                                                 \
                     "ldr r0, pc, function_const_%=\n\t"                       \
                     "gosub _start\n\t"                                        \
                     "function_const_%=: .long %0\n\t"                         \
                     :                                                         \
                     : "i"(n));                                                \
  }

typedef void (*api_func_t)(void);

#endif // EZH_H
