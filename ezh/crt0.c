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

#include "EZHRegisters.h"
#include "ezh.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

// #define __TEST__

typedef int ssize_t; // Seems to be missing from libc
typedef int (*ezh_boot_func_t)(void);

extern char __data_load_start[];
extern char __data_start[];
extern char __data_end[];
extern char __bss_start[];
extern char __bss_end[];
extern char _end;
static char *heap_end = 0;

int __llvm_libc_stdin_cookie = 0;
int __llvm_libc_stdout_cookie = 1;
int __llvm_libc_stderr_cookie = 2;
int __llvm_libc_errno_val;

#ifndef STACK_SIZE_WORDS
#define STACK_SIZE_WORDS 256
#endif

static uint32_t __attribute__((section(".stack"))) stack[STACK_SIZE_WORDS];

extern ssize_t __llvm_libc_stdio_write(void *cookie, const char *buf,
                                       size_t size);

#ifdef __TEST__
volatile int __attribute__((section(".exc_signal"))) exc_signal = 0xDEADDEAD;
volatile int wrong_result;
volatile uint32_t test_val;
int bss_test_var;

#ifndef PRINTF_BUF_SIZE
#define PRINTF_BUF_SIZE 1024
#endif
volatile char printf_buffer[PRINTF_BUF_SIZE];
volatile size_t printf_buffer_idx = 0;

#ifndef STDIN_BUF_SIZE
#define STDIN_BUF_SIZE 1024
#endif
volatile char stdin_buffer[STDIN_BUF_SIZE];
volatile size_t stdin_buffer_len = 0;
volatile size_t stdin_buffer_idx = 0;
#endif

#ifdef __EZH_BITSLICE_INTERRUPTS__
volatile uint32_t debug_frame = 0;
volatile uint32_t bitslice_cfm_backup;
#endif

void exit(int status);

int __attribute__((weak)) main(void) { return 0; }

int *__llvm_libc_errno() { return &__llvm_libc_errno_val; }

void __attribute__((section(".start"), naked)) _start() {
  __asm__ volatile("nop\n\t"
                   "nop\n\t"
                   // Disable all bit slice channel except for slice 7 (used for
                   // debug) and connect all to logical combiner
                   "ldr cfm, pc, cfm_const\n\t"
                   "ldr sp, pc, sp_const\n\t"
                   "gosub _start_c\n\t"
                   "sp_const: .long %0\n\t"
                   "cfm_const: .long %1\n\t"
                   :
                   : "i"(&stack[sizeof(stack) / sizeof(stack[0])]),
                     "i"(
#ifdef __EZH_BITSLICE_INTERRUPTS__
                         BS7(BS_SIG) |
#else
                         BS7(BS_0) |
#endif
                         BS6(BS_0) | BS5(BS_0) | BS4(BS_0) | BS3(BS_0) |
                         BS2(BS_0) | BS1(BS_0) | BS0(BS_0) | 0xFF));
}

void _start_c(ezh_boot_func_t boot_func) {
#ifdef __TEST__
  exc_signal = 0x55555555; // started
#endif

  ezh_write_cfs(BS7(EZH_INPUT_SOURCE_7) | BS6(EZH_INPUT_SOURCE_6) |
                BS5(EZH_INPUT_SOURCE_5) | BS4(EZH_INPUT_SOURCE_4) |
                BS3(EZH_INPUT_SOURCE_3) | BS2(EZH_INPUT_SOURCE_2) |
                BS1(EZH_INPUT_SOURCE_1) | BS0(EZH_INPUT_SOURCE_0) | 0x0);

  uint32_t *data_load = (uint32_t *)__data_load_start;
  uint32_t *data_vma = (uint32_t *)__data_start;
  uint32_t *data_end = (uint32_t *)__data_end;

  if (data_load != data_vma) {
    while (data_vma < data_end) {
      *data_vma++ = *data_load++;
    }
  }

  uint32_t *bss_start = (uint32_t *)__bss_start;
  uint32_t *bss_end = (uint32_t *)__bss_end;

#ifdef __TEST__
  bss_test_var = 0x12345678;
#endif
  for (uint32_t *p = bss_start; p < bss_end; p++) {
    *p = 0;
  }

#ifdef __TEST__
  exc_signal = 0xDEADB55; // BSS cleared
#endif
  __llvm_libc_errno_val = 0;

  // Natively execute all global C++ static constructors!
  extern void (*__init_array_start[])(void);
  extern void (*__init_array_end[])(void);
  size_t init_size = __init_array_end - __init_array_start;
  for (size_t i = 0; i < init_size; i++) {
    if (__init_array_start[i]) {
      __init_array_start[i]();
    }
  }

  int ret;

  if (boot_func) {
    ret = boot_func();
  } else {
    ret = main();
  }

  exit(ret);
}

void *sbrk(int incr) {
  char *prev_heap_end;
  if (heap_end == 0) {
    heap_end = (char *)&_end;
    heap_end = (char *)(((uint32_t)heap_end + 3) & ~3);
  }
  prev_heap_end = heap_end;
  heap_end += incr;
  return (void *)prev_heap_end;
}

void __attribute__((used)) exit(int status) {
#ifdef __TEST__
  // Natively execute all global C++ static destructors in reverse order!
  extern void (*__fini_array_start[])(void);
  extern void (*__fini_array_end[])(void);
  size_t fini_size = __fini_array_end - __fini_array_start;
  for (size_t i = fini_size; i > 0; i--) {
    if (__fini_array_start[i - 1]) {
      __fini_array_start[i - 1]();
    }
  }

  // Call __cxa_atexit registered destructors
  extern void __cxa_finalize(void *dso);
  __cxa_finalize(NULL);

  if (status == 0) {
    exc_signal = 0xcafebabe;
    // Natively print "exit 0\n" directly to printf_buffer
    __llvm_libc_stdio_write(NULL, "exit 0\n", 7);
  } else {
    exc_signal = status;
    // Natively print "exit 1\n" directly to printf_buffer
    __llvm_libc_stdio_write(NULL, "exit 1\n", 7);
  }
#else
  (void)status;
#endif

  while (1) {
    ezh_hold();
  }
}

void abort() {
  if (exc_signal == (int)0xDEADDEAD || exc_signal == (int)0x55555555 ||
      exc_signal == (int)0x0deadb55 || exc_signal == (int)0xFFFFFFFF) {
    exit(0xDEADC0DE);
  } else {
    exit(exc_signal);
  }
}

// Standard __cxa_atexit / destructors support
#define ATEXIT_MAX_FUNCS 32

typedef void (*destructor_t)(void *);

struct atexit_entry {
  destructor_t dest;
  void *obj;
  void *dso;
};

static struct atexit_entry atexit_funcs[ATEXIT_MAX_FUNCS];
static int atexit_func_count = 0;

void *__dso_handle = &__dso_handle;

int __cxa_atexit(destructor_t dest, void *obj, void *dso) {
  if (atexit_func_count >= ATEXIT_MAX_FUNCS) {
    return -1;
  }
  atexit_funcs[atexit_func_count++] = (struct atexit_entry){dest, obj, dso};
  return 0;
}

void __cxa_finalize(void *dso) {
  for (int i = atexit_func_count - 1; i >= 0; --i) {
    if (atexit_funcs[i].dest && (dso == NULL || atexit_funcs[i].dso == dso)) {
      atexit_funcs[i].dest(atexit_funcs[i].obj);
      atexit_funcs[i].dest = NULL; // Prevent double call
    }
  }
}

//===----------------------------------------------------------------------===//
//===----------------------------------------------------------------------===//
// Virtual Memory-Mapped Filesystem (VFS) Stub Layer for EZH Baremetal Target
//===----------------------------------------------------------------------===//
#define VFS_MAX_FILES 4
#define VFS_BUF_SIZE (32 * 1024) // 32KB per file!

typedef struct {
  char name[64];
  char data[VFS_BUF_SIZE];
  int size;
  int pos;
  int active;
} vfs_file_t;

static vfs_file_t vfs_table[VFS_MAX_FILES];
volatile int vfs_stdout_redirect_fd = -1;

// Lock-free string helper
static inline int vfs_strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static inline void vfs_strcpy(char *dest, const char *src) {
  while ((*dest++ = *src++))
    ;
}

int _open(const char *pathname, int flags, ...) {
  (void)flags;
  // 1. Search if file already exists
  for (int i = 0; i < VFS_MAX_FILES; i++) {
    if (vfs_table[i].active && vfs_strcmp(vfs_table[i].name, pathname) == 0) {
      vfs_table[i].pos = 0; // Reset write pointer (truncate)
      return i + 3;
    }
  }

  // 2. Allocate new VFS file entry
  for (int i = 0; i < VFS_MAX_FILES; i++) {
    if (!vfs_table[i].active) {
      vfs_strcpy(vfs_table[i].name, pathname);
      vfs_table[i].size = 0;
      vfs_table[i].pos = 0;
      vfs_table[i].active = 1;
      return i + 3;
    }
  }
  return -1; // VFS Overflow!
}

int _close(int fd) {
  if (fd == vfs_stdout_redirect_fd) {
    vfs_stdout_redirect_fd = -1; // Reset redirection on close!
  }
  return 0;
}

int _unlink(const char *pathname) {
  for (int i = 0; i < VFS_MAX_FILES; i++) {
    if (vfs_table[i].active && vfs_strcmp(vfs_table[i].name, pathname) == 0) {
      vfs_table[i].active = 0; // Free/delete file from VFS!
      return 0;
    }
  }
  return -1;
}

int _write(int fd, const char *buf, int len) {
  if (fd == 1 || fd == 2) {
    if (vfs_stdout_redirect_fd != -1) {
      fd = vfs_stdout_redirect_fd; // Perform VFS stdout redirection!
    } else {
      // Write directly to our trace buffer
#ifdef __TEST__
      for (int i = 0; i < len; i++) {
        printf_buffer[printf_buffer_idx] = buf[i];
        printf_buffer_idx = (printf_buffer_idx + 1) % PRINTF_BUF_SIZE;
      }
#endif
      return len;
    }
  }

  if (fd >= 3 && fd < VFS_MAX_FILES + 3) {
    vfs_file_t *file = &vfs_table[fd - 3];
    if (file->pos + len > VFS_BUF_SIZE) {
      len = VFS_BUF_SIZE - file->pos; // Truncate to buffer limit
    }
    for (int i = 0; i < len; i++) {
      file->data[file->pos + i] = buf[i];
    }
    file->pos += len;
    if (file->pos > file->size) {
      file->size = file->pos;
    }
    return len;
  }
  return -1;
}

int _read(int fd, char *buf, int len) {
  if (fd >= 3 && fd < VFS_MAX_FILES + 3) {
    vfs_file_t *file = &vfs_table[fd - 3];
    int rem = file->size - file->pos;
    if (rem <= 0) {
      return 0; // EOF
    }
    if (len > rem) {
      len = rem;
    }
    for (int i = 0; i < len; i++) {
      buf[i] = file->data[file->pos + i];
    }
    file->pos += len;
    return len;
  }
  return 0;
}

int _lseek(int fd, int offset, int whence) {
  if (fd >= 3 && fd < VFS_MAX_FILES + 3) {
    vfs_file_t *file = &vfs_table[fd - 3];
    if (whence == 0) { // SEEK_SET
      file->pos = offset;
    } else if (whence == 1) { // SEEK_CUR
      file->pos += offset;
    } else if (whence == 2) { // SEEK_END
      file->pos = file->size + offset;
    }
    if (file->pos < 0)
      file->pos = 0;
    if (file->pos > file->size)
      file->pos = file->size;
    return file->pos;
  }
  return 0;
}

int _fstat(int fd, void *buf) {
  (void)fd;
  (void)buf;
  return 0;
}

int _isatty(int fd) {
  (void)fd;
  return 1;
}

int _kill(int pid, int sig) {
  (void)pid;
  (void)sig;
  return -1;
}

int _getpid(void) { return 1; }

int clock_gettime(int clock_id, void *tp) {
  (void)clock_id;
  (void)tp;
  return -1;
}

int setenv(const char *name, const char *value, int overwrite) {
  (void)name;
  (void)value;
  (void)overwrite;
  return -1;
}

char *tmpnam(char *s) {
  static char tmpbuf[32];
  static int tmpcount = 0;
  char *p = s ? s : tmpbuf;
  char *prefix = "vfs_tmp_";
  int i = 0;
  while (prefix[i]) {
    p[i] = prefix[i];
    i++;
  }
  p[i++] = '0' + (tmpcount % 10);
  p[i++] = '.';
  p[i++] = 't';
  p[i++] = 'x';
  p[i++] = 't';
  p[i] = '\0';
  tmpcount++;
  return p;
}

void *freopen(const char *pathname, const char *mode, void *stream) {
  (void)mode;
  int fd = _open(pathname, 0);
  if (fd < 0)
    return 0;
  vfs_stdout_redirect_fd = fd;
  return stream;
}

int open(const char *pathname, int flags, ...) {
  return _open(pathname, flags);
}
int unlink(const char *pathname) { return _unlink(pathname); }
int kill(int pid, int sig) { return _kill(pid, sig); }
int getpid(void) { return _getpid(); }
int write(int fd, const char *buf, int len) { return _write(fd, buf, len); }
int read(int fd, char *buf, int len) { return _read(fd, buf, len); }
int lseek(int fd, int offset, int whence) { return _lseek(fd, offset, whence); }
int close(int fd) { return _close(fd); }
int fstat(int fd, void *buf) { return _fstat(fd, buf); }
int isatty(int fd) { return _isatty(fd); }

// Extra low-level stubs for fpcmp-target link requirements
void *fopen(const char *path, const char *mode) {
  (void)path;
  (void)mode;
  return 0;
}
int fclose(void *fp) {
  (void)fp;
  return 0;
}
int fseek(void *fp, long offset, int whence) {
  (void)fp;
  (void)offset;
  (void)whence;
  return 0;
}
long ftell(void *fp) {
  (void)fp;
  return 0;
}

double strtod(const char *nptr, char **endptr) {
  (void)nptr;
  if (endptr)
    *endptr = (char *)nptr;
  return 0.0;
}

// Master stubs to intercept standard formatting and prevent 25KB libc library
// bloat
int vsnprintf(char *str, unsigned int size, const char *format, void *ap) {
  (void)str;
  (void)size;
  (void)format;
  (void)ap;
  return 0;
}
int getrlimit(int resource, void *rlp) {
  (void)resource;
  (void)rlp;
  return 0;
}
int setrlimit(int resource, const void *rlp) {
  (void)resource;
  (void)rlp;
  return 0;
}

// Complete POSIX stubs to satisfy timeit-target link requirements
void *signal(int sig, void *func) {
  (void)sig;
  (void)func;
  return 0;
}
int fork(void) { return -1; }
int sigemptyset(void *set) {
  (void)set;
  return 0;
}
int sigaddset(void *set, int signum) {
  (void)set;
  (void)signum;
  return 0;
}
unsigned int alarm(unsigned int seconds) {
  (void)seconds;
  return 0;
}
int waitpid(int pid, int *status, int options) {
  (void)pid;
  (void)status;
  (void)options;
  return -1;
}
int setpgid(int pid, int pgid) {
  (void)pid;
  (void)pgid;
  return -1;
}
int fileno(void *stream) {
  (void)stream;
  return -1;
}
int dup2(int oldfd, int newfd) {
  (void)oldfd;
  (void)newfd;
  return -1;
}
int getrusage(int who, void *usage) {
  (void)who;
  (void)usage;
  return -1;
}
int chdir(const char *path) {
  (void)path;
  return -1;
}
int execvp(const char *file, char *const argv[]) {
  (void)file;
  (void)argv;
  return -1;
}
void perror(const char *s) { (void)s; }
void _exit(int status) {
  (void)status;
  while (1)
    ;
}
int gettimeofday(void *tv, void *tz) {
  (void)tv;
  (void)tz;
  return -1;
}

void __builtin_exit(int status) { exit(status); }

void __llvm_libc_exit(int status) { exit(status); }

#ifdef __TEST__
ssize_t __llvm_libc_stdio_write(void *cookie, const char *buf, size_t size) {
  (void)cookie;
  size_t i;
  for (i = 0; i < size; i++) {
    printf_buffer[printf_buffer_idx] = buf[i];
    printf_buffer_idx++;
    if (printf_buffer_idx >= sizeof(printf_buffer)) {
      printf_buffer_idx = 0;
    }
  }
  printf_buffer[printf_buffer_idx] = '\0'; // Ensure null-termination
  return size;
}

ssize_t __llvm_libc_stdio_read(void *cookie, char *buf, size_t size) {
  (void)cookie;
  size_t bytes_read = 0;
  while (bytes_read < size && stdin_buffer_idx < stdin_buffer_len) {
    buf[bytes_read] = stdin_buffer[stdin_buffer_idx];
    stdin_buffer_idx++;
    bytes_read++;
  }
  return bytes_read;
}
#endif

#ifdef __EZH_BITSLICE_INTERRUPTS__
/* clang-format off */
// Consolidated 3 modular assembly functions (clobber-free optimized!)
void __attribute__((naked, section(".text.bitslice_interrupts"))) debug_common() {
    __asm__ volatile (
        /* Note: ra (RA_orig) was already saved to -4 by caller! */
        /* Push GPI (1st), shifted by -4 */
        "str sp, gpi, " STR(EZH_FRAME_OFFSET_GPI) "\n\t"
        /* Push PC (2nd), shifted by -4 */
        "str sp,  ra, " STR(EZH_FRAME_OFFSET_PC) "\n\t"
        /* Push SP (3rd), shifted by -4 */
        "str sp,  sp, " STR(EZH_FRAME_OFFSET_SP) "\n\t"
        /* Push CFM (4th), shifted by -4 */
        "str sp, cfm, " STR(EZH_FRAME_OFFSET_CFM) "\n\t"
        /* Push CFS (5th), shifted by -4 */
        "str sp, cfs, " STR(EZH_FRAME_OFFSET_CFS) "\n\t"
        /* Push GPD (6th), shifted by -4 */
        "str sp, gpd, " STR(EZH_FRAME_OFFSET_GPD) "\n\t"
        /* Push GPO (7th), shifted by -4 */
        "str sp, gpo, " STR(EZH_FRAME_OFFSET_GPO) "\n\t"
        /* Push GP registers..., shifted by -4 */
        "str sp,  r7, " STR(EZH_FRAME_OFFSET_R7) "\n\t"
        "str sp,  r6, " STR(EZH_FRAME_OFFSET_R6) "\n\t"
        "str sp,  r5, " STR(EZH_FRAME_OFFSET_R5) "\n\t"
        "str sp,  r4, " STR(EZH_FRAME_OFFSET_R4) "\n\t"
        "str sp,  r3, " STR(EZH_FRAME_OFFSET_R3) "\n\t"
        "str sp,  r2, " STR(EZH_FRAME_OFFSET_R2) "\n\t"
        "str sp,  r1, " STR(EZH_FRAME_OFFSET_R1) "\n\t"
        /* Push R0 (last), shifted by -4 */
        "str sp,  r0, " STR(EZH_FRAME_OFFSET_R0) "\n\t"

        /* Build flags register dynamically in R0 */
        /* R0 = 1 (bit 0 is EU, always 1) */
        "load_imm r0, 1\n\t"
        /* Set bit 1 if ZE */
        "bset_imm_ze r0, r0, 1\n\t"
        /* Set bit 2 if NZ */
        "bset_imm_nz r0, r0, 2\n\t"
        /* Set bit 3 if PO */
        "bset_imm_po r0, r0, 3\n\t"
        /* Set bit 4 if NE */
        "bset_imm_ne r0, r0, 4\n\t"
        /* Set bit 5 if AZ */
        "bset_imm_az r0, r0, 5\n\t"
        /* Set bit 6 if ZB */
        "bset_imm_zb r0, r0, 6\n\t"
        /* Set bit 7 if CA */
        "bset_imm_ca r0, r0, 7\n\t"
        /* Set bit 8 if NC */
        "bset_imm_nc r0, r0, 8\n\t"
        /* Set bit 9 if CZ */
        "bset_imm_cz r0, r0, 9\n\t"
        /* Set bit 10 if SPO */
        "bset_imm_spo r0, r0, 10\n\t"
        /* Set bit 11 if SNE */
        "bset_imm_sne r0, r0, 11\n\t"
        /* Set bit 12 if NBS */
        "bset_imm_nbs r0, r0, 12\n\t"
        /* Set bit 13 if NEX */
        "bset_imm_nex r0, r0, 13\n\t"
        /* Set bit 14 if BS */
        "bset_imm_bs r0, r0, 14\n\t"
        /* Set bit 15 if EX */
        "bset_imm_ex r0, r0, 15\n\t"
        /* Save flags to stack offset -8 (flags near ra) */
        "str sp, r0, " STR(EZH_FRAME_OFFSET_FLAGS) "\n\t"

        /* Store SP in debug_frame using dynamic label resolution! */
        "ldr r0, pc, debug_frame_ptr\n\t"
        /* Write sp directly to debug_frame in RAM! */
        "str r0, sp, 0\n\t"

        "halt:\n\t"
        /* Read debug_frame from RAM (R0 already holds &debug_frame) */
        "ldr r1, r0, 0\n\t"
        /* Set EZH ALU flags dynamically! */
        "sub_imms r1, r1, 0\n\t"
        /* Loop back to halt if debug_frame != 0 (NZ = 1) */
        "goto_nz halt\n\t"

        /* Restore ALU flags dynamically before restoring GPRs! */
        "ldr r0, sp, " STR(EZH_FRAME_OFFSET_FLAGS) "\n\t"

        /*
         * Reconstruct 3-bit mask (ZE at bit 0, PO at bit 1, CA at bit 2)
         * from flags word
         */
        "lsr r1, r0, 1\n\t"
        /* r1 has ZE at bit 0 */
        "and_imm r1, r1, 1\n\t"

        /* shift PO (bit 3) to bit 1 (shift by 2) */
        "lsr r2, r0, 2\n\t"
        /* r2 has PO at bit 1 */
        "and_imm r2, r2, 2\n\t"
        "or r1, r1, r2\n\t"

        /* shift CA (bit 7) to bit 2 (shift by 5) */
        "lsr r2, r0, 5\n\t"
        /* r2 has CA at bit 2 */
        "and_imm r2, r2, 4\n\t"
        /* r0 has the reconstructed 3-bit mask! */
        "or r0, r1, r2\n\t"

        /* Pre-initialize r1 to 0 for restoration baseline */
        "load_imm r1, 0\n\t"

        /* Countdown flag parsing (same as bitslice_handler) */
        /* Subtract 2 to check State 2 (ZE=0, PO=1, CA=0) */
        "sub_imms r0, r0, 2\n\t"
        "goto_ze debug_restore_state_2\n\t"

        /* Subtract 1 to check State 3 (ZE=1, PO=1, CA=0) */
        "sub_imms r0, r0, 1\n\t"
        "goto_ze debug_restore_state_3\n\t"

        /* Subtract 1 to check State 4 (ZE=0, PO=0, CA=1) */
        "sub_imms r0, r0, 1\n\t"
        "goto_ze debug_restore_state_4\n\t"

        /* Subtract 2 (skipping 5) to check State 6 (ZE=0, PO=1, CA=1) */
        "sub_imms r0, r0, 2\n\t"
        "goto_ze debug_restore_state_6\n\t"

        /* Subtract 1 to check State 7 (ZE=1, PO=1, CA=1) */
        "sub_imms r0, r0, 1\n\t"
        "goto_ze debug_restore_state_7\n\t"

        /* Fallback to State 0 (ZE=0, PO=0, CA=0 -> 0x155) */
        "goto debug_restore_state_0\n\t"

        "debug_restore_state_2:\n\t"
        "load_imms r1, 1\n\t"
        "goto debug_restore_done\n\t"

        "debug_restore_state_3:\n\t"
        "load_imms r1, 0\n\t"
        "goto debug_restore_done\n\t"

        "debug_restore_state_4:\n\t"
        "sub_imms r1, r1, 1\n\t"
        "goto debug_restore_done\n\t"

        "debug_restore_state_6:\n\t"
        "sub_imms r1, r1, -1\n\t"
        "goto debug_restore_done\n\t"

        "debug_restore_state_7:\n\t"
        "sub_imm r1, r1, 1\n\t"
        "add_imms r1, r1, 1\n\t"
        "goto debug_restore_done\n\t"

        "debug_restore_state_0:\n\t"
        "load_imms r1, -1\n\t"

        "debug_restore_done:\n\t"

        /* Restores shifted by -4 */
        "ldr r0,  sp, " STR(EZH_FRAME_OFFSET_R0) "\n\t"
        "ldr r1,  sp, " STR(EZH_FRAME_OFFSET_R1) "\n\t"
        "ldr r2,  sp, " STR(EZH_FRAME_OFFSET_R2) "\n\t"
        "ldr r3,  sp, " STR(EZH_FRAME_OFFSET_R3) "\n\t"
        "ldr r4,  sp, " STR(EZH_FRAME_OFFSET_R4) "\n\t"
        "ldr r5,  sp, " STR(EZH_FRAME_OFFSET_R5) "\n\t"
        "ldr r6,  sp, " STR(EZH_FRAME_OFFSET_R6) "\n\t"
        "ldr r7,  sp, " STR(EZH_FRAME_OFFSET_R7) "\n\t"
        "ldr gpo, sp, " STR(EZH_FRAME_OFFSET_GPO) "\n\t"
        "ldr gpd, sp, " STR(EZH_FRAME_OFFSET_GPD) "\n\t"
        "ldr cfs, sp, " STR(EZH_FRAME_OFFSET_CFS) "\n\t"
        "ldr cfm, sp, " STR(EZH_FRAME_OFFSET_CFM) "\n\t"
        /* GPI restored from debug SP base (-12) */
        "ldr gpi, sp, " STR(EZH_FRAME_OFFSET_GPI) "\n\t"
        /* RA restored from debug SP base (-4) */
        "ldr ra,  sp, " STR(EZH_FRAME_OFFSET_RA) "\n\t"
        /* Restore SP to original base! */
        "ldr sp,  sp, " STR(EZH_FRAME_OFFSET_SP) "\n\t"
        /* Load PC (original_sp - 20) to jump return! */
        "ldr pc,  sp, " STR(EZH_FRAME_OFFSET_PC) "\n\t"

        "debug_frame_ptr: .long debug_frame\n\t"
    );
}

void __attribute__((naked, section(".text.bitslice_interrupts"))) debug_call() {
    __asm__ volatile (
        // Save return RA (return PC) to stack offset -4 instantly! (0-Clobber!)
        "str sp,  ra, " STR(EZH_FRAME_OFFSET_RA) "\n\t"
        // Jump directly to common handler!
        "goto debug_common\n\t"
    );
}

// Defines a software breakpoint vector
// Software breakpoints work by lldb swapping out an instruction with a
// goto debug_software_breakpoint_[n] with n being the breakpoint number. The 
// challenge is when a the goto is taken, we have no record on where we came
// from. Software breakpoints can be placed anywhere. We can't use gotol 
// because this may clobber an RA that hasn't been been pushed to the stack yet.
// EZH has no instructions that can push to the stack and jump in the same 
// instruction. To work around these limitations, 16 unique software breakpoints
// are created that push the original RA to the stack and set RA to 0xFFFFFFF[n]
// with n being the breakpoint number. LLDB knows the address that it set for
// each  breakpoint. Using the RA value, it can figure out what breakpoint hit
// and where PC was when it hit.
#define SW_BP_HANDLER(slot) \
void __attribute__((naked, section(".text.bitslice_interrupts"))) debug_software_breakpoint_##slot() { \
    __asm__ volatile ( \
        "str sp,  ra, " STR(EZH_FRAME_OFFSET_RA) "\n\t" \
        "load_imm ra, " STR(-16 + slot) "\n\t" \
        "goto debug_common\n\t" \
    ); \
}

SW_BP_HANDLER(0)
SW_BP_HANDLER(1)
SW_BP_HANDLER(2)
SW_BP_HANDLER(3)
SW_BP_HANDLER(4)
SW_BP_HANDLER(5)
SW_BP_HANDLER(6)
SW_BP_HANDLER(7)
SW_BP_HANDLER(8)
SW_BP_HANDLER(9)
SW_BP_HANDLER(10)
SW_BP_HANDLER(11)
SW_BP_HANDLER(12)
SW_BP_HANDLER(13)
SW_BP_HANDLER(14)
SW_BP_HANDLER(15)

// Link-Time Debugger Veneer Reservation.
//
// The function created with this macro is never executed. It's purpose is 
// exclusively to force the linker to generate thunks/veneers to the software 
// breakpoints vectors. 
//
// Goto instructions only have 8MB of range. When LLDB injects a software
// breakpoint it will first attempt to use the breakpoints vector declared above
// directly if the code and the breakpoints vectors are in the 8MB page. If not,
// LLDB will use a thunk/veneer to get to the breakpoint vector. If no in range 
// thunk is found, breakpoint creation will fail. This means you must declare 
// and KEEP this function for every used 8MB page.
#define EZH_DEFINE_DEBUG_BREAKPOINT_RESERVE(func_name) \
void __attribute__((naked, section(".text." #func_name))) func_name(void) { \
    __asm__ volatile ( \
        "goto debug_software_breakpoint_0\n\t" \
        "goto debug_software_breakpoint_1\n\t" \
        "goto debug_software_breakpoint_2\n\t" \
        "goto debug_software_breakpoint_3\n\t" \
        "goto debug_software_breakpoint_4\n\t" \
        "goto debug_software_breakpoint_5\n\t" \
        "goto debug_software_breakpoint_6\n\t" \
        "goto debug_software_breakpoint_7\n\t" \
        "goto debug_software_breakpoint_8\n\t" \
        "goto debug_software_breakpoint_9\n\t" \
        "goto debug_software_breakpoint_10\n\t" \
        "goto debug_software_breakpoint_11\n\t" \
        "goto debug_software_breakpoint_12\n\t" \
        "goto debug_software_breakpoint_13\n\t" \
        "goto debug_software_breakpoint_14\n\t" \
        "goto debug_software_breakpoint_15\n\t" \
    ); \
}

EZH_DEFINE_DEBUG_BREAKPOINT_RESERVE(debug_breakpoint_reserve_ram)

// Weak stubs for bitslice vectors
void __attribute__((weak)) vector0() {}
void __attribute__((weak)) vector1() {}
void __attribute__((weak)) vector2() {}
void __attribute__((weak)) vector3() {}
void __attribute__((weak)) vector4() {}
void __attribute__((weak)) vector5() {}
void __attribute__((weak)) vector6() {}
void __attribute__((weak)) vector7() {
    debug_call();
}

void __attribute__((naked, section(".text.bitslice_interrupts"))) bitslice_handler() {
    __asm__ volatile (
        ".cfi_startproc\n\t"
        /* Save original RA (which is in RA because gotol saves PC to RA) */
        /* Save original RA to stack offset -4 */
        "pushd ra\n\t"
        ".cfi_def_cfa_offset 4\n\t"
        ".cfi_offset 15, -4\n\t"

        /* Save GPRs and registers we might clobber.
           We save R0-R3 because they are caller-saved in ABI
           and might be clobbered by C functions called from here.
        */
        "pushd r0\n\t"
        ".cfi_def_cfa_offset 8\n\t"
        ".cfi_offset 0, -8\n\t"
        "pushd r1\n\t"
        ".cfi_def_cfa_offset 12\n\t"
        ".cfi_offset 1, -12\n\t"
        "pushd r2\n\t"
        ".cfi_def_cfa_offset 16\n\t"
        ".cfi_offset 2, -16\n\t"
        "pushd r3\n\t"
        ".cfi_def_cfa_offset 20\n\t"
        ".cfi_offset 3, -20\n\t"

        /* Save ALU flags dynamically.
           We only capture ZE (bit 0), PO (bit 1), and CA (bit 2)
           consecutively to ensure contiguous countdown mask values.
         */
        "load_imm r0, 0\n\t"
        "bset_imm_ze r0, r0, 0\n\t"
        "bset_imm_po r0, r0, 1\n\t"
        "bset_imm_ca r0, r0, 2\n\t"
        /* Save flags mask to stack offset -24 */
        "pushd r0\n\t"
        ".cfi_def_cfa_offset 24\n\t"

        /* Backup original CFM configuration to global RAM variable */
        "ldr r0,  pc, bitslice_cfm_backup_ptr\n\t"
        "str r0,  cfm, 0\n\t"

        /*
         * Read CFS to see what bit slices are activated (lower 8 bits) into
         * R1
         */
        "mov r1, cfs\n\t"
        "per_read r0, " STR(EZHB_PENDTRAP) "\n\t"
        "or r1, r1, r0\n\t"

        /* Deactivate all bit slices immediately to prevent recursive
           bitslice interrupts during handler/vector execution.
        */
        "lsr r0, cfm, 8\n\t"
        "lsl cfm, r0, 8\n\t"

        /* Check each bit of CFS [7:0] and call active vectors directly
           using conditional branch with link (gotol_nz).
           gotol_nz automatically updates RA with the return address.
        */

        /* Check bit 0 (vector0) */
        "btst_imms r0, r1, 0\n\t"
        "gotol_nz vector0\n\t"

        /* Check bit 1 (vector1) */
        "btst_imms r0, r1, 1\n\t"
        "gotol_nz vector1\n\t"

        /* Check bit 2 (vector2) */
        "btst_imms r0, r1, 2\n\t"
        "gotol_nz vector2\n\t"

        /* Check bit 3 (vector3) */
        "btst_imms r0, r1, 3\n\t"
        "gotol_nz vector3\n\t"

        /* Check bit 4 (vector4) */
        "btst_imms r0, r1, 4\n\t"
        "gotol_nz vector4\n\t"

        /* Check bit 5 (vector5) */
        "btst_imms r0, r1, 5\n\t"
        "gotol_nz vector5\n\t"

        /* Check bit 6 (vector6) */
        "btst_imms r0, r1, 6\n\t"
        "gotol_nz vector6\n\t"

        /* Check bit 7 (vector7) */
        "btst_imms r0, r1, 7\n\t"
        "gotol_nz vector7\n\t"

        /* Restore ALU flags.
           Load packed flags from stack into R0.
           Pre-initialize R1 to 0 to serve as a zero baseline for flag
           restoration.
        */
        "popd r0\n\t"
        "load_imm r1, 0\n\t"

        /* Compressed Countdown flag parsing:
           We progressively decrement R0 in a linear chain, combining skips
           over impossible states into single subtractions of 2.
           Layout: bit 0: ZE, bit 1: PO, bit 2: CA
        */
        /* Subtract 2 to check State 2 (ZE=0, PO=1, CA=0) */
        "sub_imms r0, r0, 2\n\t"
        "goto_ze restore_state_2\n\t"

        /* Subtract 1 to check State 3 (ZE=1, PO=1, CA=0) */
        "sub_imms r0, r0, 1\n\t"
        "goto_ze restore_state_3\n\t"

        /* Subtract 1 to check State 4 (ZE=0, PO=0, CA=1) */
        "sub_imms r0, r0, 1\n\t"
        "goto_ze restore_state_4\n\t"

        /* Subtract 2 (skipping 5) to check State 6 (ZE=0, PO=1, CA=1) */
        "sub_imms r0, r0, 2\n\t"
        "goto_ze restore_state_6\n\t"

        /* Subtract 1 to check State 7 (ZE=1, PO=1, CA=1) */
        "sub_imms r0, r0, 1\n\t"
        "goto_ze restore_state_7\n\t"

        /* Fallback if initially 0 or invalid */
        "goto restore_state_0\n\t"

        /*
         * State 0: ZE=0, PO=0, CA=0 -> load_imms r1, -1 (sets ZE=0, PO=0,
         * CA=0)
         */
        "restore_state_0:\n\t"
        "load_imms r1, -1\n\t"
        "goto restore_done\n\t"

        /*
         * State 2: ZE=0, PO=1, CA=0 -> load_imms r1, 1 (sets ZE=0, PO=1,
         * CA=0)
         */
        "restore_state_2:\n\t"
        "load_imms r1, 1\n\t"
        "goto restore_done\n\t"

        /*
         * State 3: ZE=1, PO=1, CA=0 -> load_imms r1, 0 (sets ZE=1, PO=1,
         * CA=0)
         */
        "restore_state_3:\n\t"
        "load_imms r1, 0\n\t"
        "goto restore_done\n\t"

        /* State 4: ZE=0, PO=0, CA=1 -> execute R1 = 0 - 1 */
        "restore_state_4:\n\t"
        /* R1 is already 0 */
        "sub_imms r1, r1, 1\n\t"
        "goto restore_done\n\t"

        /* State 6: ZE=0, PO=1, CA=1 -> execute R1 = 0 - (-1) */
        "restore_state_6:\n\t"
        /* R1 is already 0 -> 0 - (-1) = 1 */
        "sub_imms r1, r1, -1\n\t"
        "goto restore_done\n\t"

        /* State 7: ZE=1, PO=1, CA=1 -> execute R1 = 0xFFFFFFFF + 1 */
        "restore_state_7:\n\t"
        /* R1 = 0 - 1 = 0xFFFFFFFF (-1) */
        "sub_imm r1, r1, 1\n\t"
        "add_imms r1, r1, 1\n\t"

        "restore_done:\n\t"

        /* Restore CFM configuration from backup */
        "ldr r0,  pc, bitslice_cfm_backup_ptr\n\t"
        "ldr cfm, r0, 0\n\t"

        /* Restore GPRs */
        "popd r3\n\t"
        "popd r2\n\t"
        "popd r1\n\t"
        "popd r0\n\t"

        /* Restore RA and return */
        "popd ra\n\t"
        "mov pc, ra\n\t"

        "bitslice_cfm_backup_ptr: .long bitslice_cfm_backup\n\t"
        ".cfi_endproc\n\t"
    );
}
/* clang-format on */
#endif
