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

#ifndef _SHIM_STDLIB_H
#define _SHIM_STDLIB_H

#include_next <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// 1. Global C-linkage declarations
extern void *realloc(void *ptr, size_t size);
extern double strtod(const char *nptr, char **endptr);
extern int setenv(const char *name, const char *value, int overwrite);

inline int system(const char *cmd) {
  (void)cmd;
  return 0;
}

inline char *getenv(const char *name) {
  (void)name;
  return 0;
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif
