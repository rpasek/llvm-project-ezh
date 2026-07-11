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

#ifndef _SHIM_WCTYPE_H
#define _SHIM_WCTYPE_H

#include <locale.h> // for locale_t
#include <string.h> // for strcmp
#include_next <wctype.h>

// 1. Custom wctype_t type and standard iswctype engine
#ifndef LLVM_LIBC_TYPES_WCTYPE_T_H
typedef unsigned int wctype_t;
#endif

inline wctype_t wctype(const char *name) {
  if (strcmp(name, "alnum") == 0)
    return 1;
  if (strcmp(name, "alpha") == 0)
    return 2;
  if (strcmp(name, "blank") == 0)
    return 3;
  if (strcmp(name, "cntrl") == 0)
    return 4;
  if (strcmp(name, "digit") == 0)
    return 5;
  if (strcmp(name, "graph") == 0)
    return 6;
  if (strcmp(name, "lower") == 0)
    return 7;
  if (strcmp(name, "print") == 0)
    return 8;
  if (strcmp(name, "punct") == 0)
    return 9;
  if (strcmp(name, "space") == 0)
    return 10;
  if (strcmp(name, "upper") == 0)
    return 11;
  if (strcmp(name, "xdigit") == 0)
    return 12;
  return 0;
}

inline int iswctype(wint_t wc, wctype_t desc) {
  switch (desc) {
  case 1:
    return iswalnum(wc);
  case 2:
    return iswalpha(wc);
  case 3:
    return iswblank(wc);
  case 4:
    return iswcntrl(wc);
  case 5:
    return iswdigit(wc);
  case 6:
    return iswgraph(wc);
  case 7:
    return iswlower(wc);
  case 8:
    return iswprint(wc);
  case 9:
    return iswpunct(wc);
  case 10:
    return iswspace(wc);
  case 11:
    return iswupper(wc);
  case 12:
    return iswxdigit(wc);
  }
  return 0;
}

// 2. Custom wctrans_t type and standard wide-character mapping stubs
typedef unsigned long wctrans_t;

inline wctrans_t wctrans(const char *name) {
  (void)name;
  return 0;
}
inline wint_t towctrans(wint_t wc, wctrans_t desc) {
  (void)desc;
  return wc;
}

// 3. Custom inline implementations of missing towlower and towupper for ASCII
inline wint_t towlower(wint_t wc) {
  return (wc >= 'A' && wc <= 'Z') ? (wc - 'A' + 'a') : wc;
}
inline wint_t towupper(wint_t wc) {
  return (wc >= 'a' && wc <= 'z') ? (wc - 'a' + 'A') : wc;
}

// 4. Custom inline wrappers for C standard wide-character locale-specific
// classification APIs
inline int iswalnum_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswalnum(wc);
}
inline int iswalpha_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswalpha(wc);
}
inline int iswblank_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswblank(wc);
}
inline int iswcntrl_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswcntrl(wc);
}
inline int iswdigit_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswdigit(wc);
}
inline int iswgraph_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswgraph(wc);
}
inline int iswlower_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswlower(wc);
}
inline int iswprint_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswprint(wc);
}
inline int iswpunct_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswpunct(wc);
}
inline int iswspace_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswspace(wc);
}
inline int iswupper_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswupper(wc);
}
inline int iswxdigit_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswxdigit(wc);
}

inline wint_t towlower_l(wint_t wc, locale_t locale) {
  (void)locale;
  return towlower(wc);
}
inline wint_t towupper_l(wint_t wc, locale_t locale) {
  (void)locale;
  return towupper(wc);
}

inline int iswctype_l(wint_t wc, wctype_t desc, locale_t locale) {
  (void)locale;
  return iswctype(wc, desc);
}

#endif
