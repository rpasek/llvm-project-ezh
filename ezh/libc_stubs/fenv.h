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

#ifndef _SHIM_FENV_H
#define _SHIM_FENV_H

typedef unsigned int fenv_t;
typedef unsigned int fexcept_t;

#define FE_TONEAREST 0
#define FE_DOWNWARD 1
#define FE_UPWARD 2
#define FE_TOWARDZERO 3

#define FE_ALL_EXCEPT 0

extern int fegetenv(fenv_t *envp);
extern int fesetenv(const fenv_t *envp);
extern int feholdexcept(fenv_t *envp);
extern int feupdateenv(const fenv_t *envp);
extern int fetestexcept(int excepts);
extern int feclearexcept(int excepts);
extern int feraiseexcept(int excepts);
extern int fegetround(void);
extern int fesetround(int round);

#endif
