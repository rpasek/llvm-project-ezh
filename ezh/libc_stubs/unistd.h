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

#ifndef _UNISTD_H
#define _UNISTD_H

#include <sys/types.h>

#define __BEGIN_DECLS
#define __END_DECLS
#define __THROW
#define __nonnull(x)
#define __wur

#define STDOUT_FILENO 1
#define STDERR_FILENO 2

extern int access(const char *name, int type);
extern off_t lseek(int fd, off_t offset, int whence);
extern ssize_t read(int fd, void *buf, size_t nbytes);
extern ssize_t write(int fd, const void *buf, size_t n);
extern int pipe(int pipedes[2]);
extern unsigned int alarm(unsigned int seconds);
extern int chdir(const char *path);
extern char *getcwd(char *buf, size_t size);
extern int dup(int fd);
extern int dup2(int fd, int fd2);
extern int close(int fd);

extern int setpgid(pid_t pid, pid_t pgid);
extern void _exit(int status);
extern int kill(pid_t pid, int sig);
extern int execvp(const char *file, char *const argv[]);
extern pid_t fork(void);
extern int isatty(int fd);

#endif // _UNISTD_H
