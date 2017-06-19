/* -*- mode:C++; -*- */
/* MIT License -- MyThOS: The Many-Threads Operating System
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
 * Copyright 2016 Randolf Rotta, Robert Kuban, Maik Krüger, and contributors, BTU Cottbus-Senftenberg
 */
#pragma once

#include "util/compiler.hh"
#include "cpu/hwthreadid.hh"

namespace mythos {
  namespace idle {

    /** called once on bootup by the BSP. Initialises the trampoline and core states. */
    void init_global() {}

    /** called once on bootup on each AP to initialise the processor, if needed. */
    void init_thread() {}

    /** dependency: has to be implemented by kernel */
    NORETURN void sleeping_failed() SYMBOL("sleeping_failed");

    /** low level assembler routine that halts the hardware thread. */
    NORETURN void cpu_idle_halt() SYMBOL("cpu_idle_halt");

    /** The kernel has nothing to do, thus go sleeping.
     *
     * High level idle governers may replace this function. (somehow)
     * resets the kernel stack.
     */
    NORETURN void sleep() { cpu_idle_halt(); }

    /** sleep management event: awakened by booting or from deep sleep. */
    void wokeup(size_t /*apicID*/, size_t /*reason*/) {}

    /** sleep management event: awakened by interrupt, possibly from light sleep */
    void wokeupFromInterrupt() {}

    /** sleep management event: entered kernel from syscall */
    void enteredFromSyscall() {}

    /** sleep management event: entered kernel from interrupting the user mode */
    void enteredFromInterrupt() {}

  } // namespace idle
} // namespace mythos
