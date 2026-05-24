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

#ifndef _SHIM_STDIO_H
#define _SHIM_STDIO_H

#include_next <stdio.h>

#include <stdarg.h>

extern FILE *stderr;
extern FILE *stdout;

extern int fprintf(FILE *stream, const char *format, ...);
extern int fclose(FILE *stream);
extern FILE *fopen(const char *filename, const char *mode);
extern size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
extern size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
extern int fseek(FILE *stream, long offset, int whence);
extern long ftell(FILE *stream);
extern void rewind(FILE *stream);
extern void perror(const char *s);
extern int fileno(FILE *stream);
extern int feof(FILE *stream);
extern int ferror(FILE *stream);
extern int remove(const char *filename);
extern int vsnprintf(char *str, size_t size, const char *format, va_list ap);

#endif
