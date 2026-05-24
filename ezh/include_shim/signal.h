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

#ifndef _SIGNAL_H
#define _SIGNAL_H

#define __BEGIN_DECLS
#define __END_DECLS
#define __THROW

typedef unsigned int sigset_t;

#define SIGINT 2
#define SIGKILL 9
#define SIGALRM 14
#define SIGTERM 15

typedef void (*sighandler_t)(int);

extern int raise(int sig);
extern int sigemptyset(sigset_t *set);
extern int sigaddset(sigset_t *set, int signo);
extern sighandler_t signal(int signum, sighandler_t handler);

#endif
