/* -*- mode:C++; -*- */
/* MyThOS: The Many-Threads Operating System
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright 2016 Randolf Rotta, Robert Kuban, and contributors, BTU Cottbus-Senftenberg
 */

#include "stdlib.h"
#include <cstddef>
#include <atomic>

extern void (*__preinit_array_start []) (void) __attribute__((weak));
extern void (*__preinit_array_end []) (void) __attribute__((weak));
extern void (*__init_array_start []) (void) __attribute__((weak));
extern void (*__init_array_end []) (void) __attribute__((weak));
extern void (*__fini_array_start []) (void) __attribute__((weak));
extern void (*__fini_array_end []) (void) __attribute__((weak));
extern "C" void _init();
extern "C" void _fini();

extern "C" void __libc_init_array()
{
  for (size_t i = 0; i < __preinit_array_end - __preinit_array_start; i++)
    __preinit_array_start[i]();
  _init();
  for (size_t i = 0; i < __init_array_end - __init_array_start; i++)
    __init_array_start[i]();
}

static void __libc_fini_array()
{
  for (size_t i = __fini_array_end - __fini_array_start; i > 0; i--)
    __fini_array_start[i-1]();
  _fini();
}

constexpr size_t atexit_funcs_max = 32;
static std::atomic<size_t> atexit_funcs_count = {0};
static atexit_func_t atexit_funcs[atexit_funcs_max];

extern "C" int atexit(atexit_func_t func)
{
  size_t pos = atexit_funcs_count.fetch_add(1);
  if (pos >= atexit_funcs_max) {
    atexit_funcs_count = atexit_funcs_max;
    return -1;
  }
  atexit_funcs[pos] = func;
  return 0;
}

void _Exit(int exit_code)
{
  asm volatile ("syscall" : : "D"(0), "S"(exit_code) : "memory");
  while(true);
}

void exit(int exit_code)
{
  for (size_t i = 0; i < atexit_funcs_count; i++)
    if (atexit_funcs[i]) atexit_funcs[i]();
  __libc_fini_array();
  _Exit(exit_code);
}
