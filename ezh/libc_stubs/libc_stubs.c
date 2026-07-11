/* Copyright 2026 Google LLC
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

#include <stddef.h>
#include <stdint.h>

typedef int ssize_t;

#ifndef STDOUT_BUF_SIZE
#define STDOUT_BUF_SIZE 4096
#endif // STOUT_BUF_SIZE

#ifndef STDIN_BUF_SIZE
#define STDIN_BUF_SIZE 1024
#endif // STDIN_BUF_SIZE

/* Standard stream cookies required by LLVM libc stdio */
int __llvm_libc_stdin_cookie = 0;
int __llvm_libc_stdout_cookie = 1;
int __llvm_libc_stderr_cookie = 2;

/* Global buffers for remote debugger console stdout/stdin sync */
volatile char stdout_buffer[STDOUT_BUF_SIZE];
volatile size_t stdout_buffer_idx = 0;

volatile char stdin_buffer[STDIN_BUF_SIZE];
volatile size_t stdin_buffer_len = 0;
volatile size_t stdin_buffer_idx = 0;

/* errno implementation */
int __llvm_libc_errno_val;
int *__llvm_libc_errno(void) { return &__llvm_libc_errno_val; }

/* sbrk memory allocator */
extern char _end;
static char *heap_end = 0;
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

/* Helper string match functions */
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

/* System Call Implementation Stubs */
int _open(const char *pathname, int flags, ...) {
  (void)flags;
  for (int i = 0; i < VFS_MAX_FILES; i++) {
    if (vfs_table[i].active && vfs_strcmp(vfs_table[i].name, pathname) == 0) {
      vfs_table[i].pos = 0; // Truncate on write-open
      return i + 3;
    }
  }

  for (int i = 0; i < VFS_MAX_FILES; i++) {
    if (!vfs_table[i].active) {
      vfs_strcpy(vfs_table[i].name, pathname);
      vfs_table[i].size = 0;
      vfs_table[i].pos = 0;
      vfs_table[i].active = 1;
      return i + 3;
    }
  }
  return -1;
}

int _close(int fd) {
  if (fd == vfs_stdout_redirect_fd) {
    vfs_stdout_redirect_fd = -1;
  }
  return 0;
}

int _unlink(const char *pathname) {
  for (int i = 0; i < VFS_MAX_FILES; i++) {
    if (vfs_table[i].active && vfs_strcmp(vfs_table[i].name, pathname) == 0) {
      vfs_table[i].active = 0;
      return 0;
    }
  }
  return -1;
}

int _write(int fd, const char *buf, int len) {
  if (fd == 1 || fd == 2) {
    if (vfs_stdout_redirect_fd != -1) {
      fd = vfs_stdout_redirect_fd;
    } else {
      for (int i = 0; i < len; i++) {
        stdout_buffer[stdout_buffer_idx] = buf[i];
        stdout_buffer_idx = (stdout_buffer_idx + 1) % STDOUT_BUF_SIZE;
      }
      return len;
    }
  }

  if (fd >= 3 && fd < VFS_MAX_FILES + 3) {
    vfs_file_t *file = &vfs_table[fd - 3];
    if (file->pos + len > VFS_BUF_SIZE) {
      len = VFS_BUF_SIZE - file->pos;
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
      return 0;
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
    if (whence == 0) {
      file->pos = offset;
    } else if (whence == 1) {
      file->pos += offset;
    } else if (whence == 2) {
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

/* Posix Wrapping Stubs */
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

int open(const char *pathname, int flags, ...) { return _open(pathname, flags); }
int unlink(const char *pathname) { return _unlink(pathname); }
int kill(int pid, int sig) { return _kill(pid, sig); }
int getpid(void) { return _getpid(); }
int write(int fd, const char *buf, int len) { return _write(fd, buf, len); }
int read(int fd, char *buf, int len) { return _read(fd, buf, len); }
int lseek(int fd, int offset, int whence) { return _lseek(fd, offset, whence); }
int close(int fd) { return _close(fd); }
int fstat(int fd, void *buf) { return _fstat(fd, buf); }
int isatty(int fd) { return _isatty(fd); }

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

void exit(int status);
void __builtin_exit(int status) { exit(status); }
void __llvm_libc_exit(int status) { exit(status); }

ssize_t __llvm_libc_stdio_write(void *cookie, const char *buf, size_t size) {
  (void)cookie;
  size_t i;
  for (i = 0; i < size; i++) {
    stdout_buffer[stdout_buffer_idx] = buf[i];
    stdout_buffer_idx++;
    if (stdout_buffer_idx >= sizeof(stdout_buffer)) {
      stdout_buffer_idx = 0;
    }
  }
  stdout_buffer[stdout_buffer_idx] = '\0';
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
