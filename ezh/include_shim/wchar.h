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

#ifndef _SHIM_WCHAR_H
#define _SHIM_WCHAR_H

#include <locale.h> // for locale_t
#include <stddef.h> // for size_t
#include <stdio.h>  // for FILE, stdout, stderr, fileno
#include <unistd.h> // for write
#include_next <wchar.h>

// 1. Custom inline implementations of missing C-standard wcsxfrm_l for C-locale
// fallback
inline size_t wcsxfrm_l(wchar_t *dest, const wchar_t *src, size_t n,
                        locale_t locale) {
  (void)locale;
  size_t len = 0;
  const wchar_t *s = src;
  while (*s++)
    len++;
  if (dest && n > 0) {
    size_t copy_len = (len < n) ? len : (n - 1);
    for (size_t i = 0; i < copy_len; i++) {
      dest[i] = src[i];
    }
    dest[copy_len] = L'\0';
  }
  return len;
}

inline size_t wcsxfrm(wchar_t *dest, const wchar_t *src, size_t n) {
  return wcsxfrm_l(dest, src, n, NULL);
}

// 2. Custom inline implementation of missing wcscoll and wcscoll_l (falls back
// to wcscmp)
inline int wcscoll(const wchar_t *s1, const wchar_t *s2) {
  return wcscmp(s1, s2);
}
inline int wcscoll_l(const wchar_t *s1, const wchar_t *s2, locale_t locale) {
  (void)locale;
  return wcscmp(s1, s2);
}

// 3. Complete wide character standard IO inline stubs mapping to EZH VFS layer
inline wint_t fputwc(wchar_t wc, FILE *stream) {
  char c = (char)wc;
  if (write(fileno(stream), &c, 1) == 1) {
    return wc;
  }
  return WEOF;
}

inline wint_t ungetwc(wint_t wc, FILE *stream) {
  (void)stream;
  return wc;
}

inline wint_t fgetwc(FILE *stream) {
  (void)stream;
  return WEOF;
}

inline wchar_t *fgetws(wchar_t *__restrict s, int n, FILE *__restrict stream) {
  (void)s;
  (void)n;
  (void)stream;
  return NULL;
}

inline int fputws(const wchar_t *__restrict s, FILE *__restrict stream) {
  (void)s;
  (void)stream;
  return -1;
}

inline int fwide(FILE *stream, int mode) {
  (void)stream;
  (void)mode;
  return 0;
}

inline wint_t getwc(FILE *stream) {
  (void)stream;
  return WEOF;
}

inline wint_t putwc(wchar_t c, FILE *stream) { return fputwc(c, stream); }

inline wint_t getwchar(void) { return WEOF; }

inline wint_t putwchar(wchar_t c) { return fputwc(c, stdout); }

// Wide format print stubs to satisfy link requirements
inline int fwprintf(FILE *__restrict stream, const wchar_t *__restrict format,
                    ...) {
  (void)stream;
  (void)format;
  return 0;
}
inline int fwscanf(FILE *__restrict stream, const wchar_t *__restrict format,
                   ...) {
  (void)stream;
  (void)format;
  return 0;
}
inline int swprintf(wchar_t *__restrict s, size_t n,
                    const wchar_t *__restrict format, ...) {
  (void)s;
  (void)n;
  (void)format;
  return 0;
}
inline int swscanf(const wchar_t *__restrict s,
                   const wchar_t *__restrict format, ...) {
  (void)s;
  (void)format;
  return 0;
}
inline int vfwprintf(FILE *__restrict stream, const wchar_t *__restrict format,
                     void *arg) {
  (void)stream;
  (void)format;
  (void)arg;
  return 0;
}
inline int vswprintf(wchar_t *__restrict s, size_t n,
                     const wchar_t *__restrict format, void *arg) {
  (void)s;
  (void)n;
  (void)format;
  (void)arg;
  return 0;
}
inline int vswscanf(const wchar_t *__restrict s,
                    const wchar_t *__restrict format, void *arg) {
  (void)s;
  (void)format;
  (void)arg;
  return 0;
}
inline int vfwscanf(FILE *__restrict stream, const wchar_t *__restrict format,
                    void *arg) {
  (void)stream;
  (void)format;
  (void)arg;
  return 0;
}
inline int vwscanf(const wchar_t *__restrict format, void *arg) {
  (void)format;
  (void)arg;
  return 0;
}
inline int wscanf(const wchar_t *__restrict format, ...) {
  (void)format;
  return 0;
}
inline int vwprintf(const wchar_t *__restrict format, void *arg) {
  (void)format;
  (void)arg;
  return 0;
}
inline int wprintf(const wchar_t *__restrict format, ...) {
  (void)format;
  return 0;
}

#endif
