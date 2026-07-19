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

/*
 * On-board self-test for EZH tail-call and musttail forwarding: exits 0
 * (exc_signal 0xCAFEBABE) iff every shape computes the right value at
 * runtime. Shapes: stack-argument forward (5 args), stack-argument swap
 * (6 args), vararg ellipsis forward (musttail_fwd.ll), indirect table
 * dispatch, a deep sibling chain, and the memory-form musttail (indirect
 * with all four argument registers occupied: target parked in a stack
 * slot, loaded into PC after the frame teardown). Compiled with bitslice
 * interrupts on, so every tail call also exercises the
 * poll-before-epilogue placement. Runner: run_musttail.sh.
 */
volatile unsigned r1, r2, r3, r4, r5, r6, r7;

static int __attribute__((noinline)) g5(int a, int b, int c, int d, int e) {
  return a + 2*b + 3*c + 4*d + 5*e;
}
static int __attribute__((noinline)) f5(int a, int b, int c, int d, int e) {
  __attribute__((musttail)) return g5(a, b, c, d, e + 1);
}

static int __attribute__((noinline)) g6(int a, int b, int c, int d, int e, int f) {
  return a + 2*b + 3*c + 4*d + 5*e + 6*f;
}
static int __attribute__((noinline)) swap6(int a, int b, int c, int d, int e, int f) {
  __attribute__((musttail)) return g6(a, b, c, d, f, e); /* swap stack args */
}

#include <stdarg.h>
int __attribute__((noinline, used)) vsum(int n, ...) {
  va_list ap; va_start(ap, n);
  int s = 0;
  for (int i = 0; i < n; i++) s += va_arg(ap, int);
  va_end(ap);
  return s;
}
int vfwd(int n, ...); /* defined in musttail_fwd.ll: musttail forwards ... to vsum */
int vsum_entry(int n, ...) __attribute__((alias("vsum")));

typedef int (*fn1)(int);
static int __attribute__((noinline)) inc3(int x) { return x + 3; }
static int __attribute__((noinline)) dbl(int x) { return x * 2; }
static int __attribute__((noinline)) dispatch(fn1 f, int x) { return f(x); }
volatile fn1 table[2] = {inc3, dbl};

/* all four argument registers occupied + indirect target: the memory-form
 * musttail (target parked in a stack slot, loaded into PC after teardown) */
typedef int (*fn4)(int, int, int, int);
static int __attribute__((noinline)) sum4(int a, int b, int c, int d) {
  return a + 2*b + 3*c + 4*d;
}
volatile fn4 fp4 = sum4;
static int __attribute__((noinline)) must4(int a, int b, int c, int d) {
  __attribute__((musttail)) return fp4(a, b, c, d);
}

/* memory-form musttail from a large frame: the target slot is pinned at
 * the top of the frame, so the post-teardown load stays in range even
 * with 600 bytes of locals */
static int __attribute__((noinline)) must4_large(int a, int b, int c, int d) {
  volatile char buf[600];
  buf[599] = 1;
  __attribute__((musttail)) return fp4(a + buf[599], b, c, d);
}

/* deep sibling chain: also exercises the poll-before-epilogue path */
static int __attribute__((noinline)) c3(int x) { return x + 100; }
static int __attribute__((noinline)) c2(int x) { return c3(x + 10); }
static int __attribute__((noinline)) c1(int x) { return c2(x + 1); }

volatile int in1 = 1, in2 = 2, in3 = 3, in4 = 4, in5 = 5, in6 = 6, in7 = 7, inx = 5;

int main(void) {
  r1 = (unsigned)f5(in1, in2, in3, in4, in5);        /* g5(1,2,3,4,6): 1+4+9+16+30 = 60 */
  r2 = (unsigned)swap6(in1, in2, in3, in4, in5, in6);  /* g6(1,2,3,4,6,5): 1+4+9+16+30+30 = 90 */
  r3 = (unsigned)vfwd(in3, 10 * in1, 10 * in2, 10 * in3);      /* 60 */
  r4 = (unsigned)(dispatch(table[0], in7) + dispatch(table[1], in7)); /* 10+14 = 24 */
  r5 = (unsigned)c1(inx);                    /* 116 */
  r6 = (unsigned)must4(in1, in2, in3, in4);  /* 1+4+9+16 = 30 */
  r7 = (unsigned)must4_large(in1, in2, in3, in4); /* sum4(2,2,3,4) = 2+4+9+16 = 31 */
  return (r1 == 60 && r2 == 90 && r3 == 60 && r4 == 24 && r5 == 116 && r6 == 30 && r7 == 31) ? 0 : 1;
}
