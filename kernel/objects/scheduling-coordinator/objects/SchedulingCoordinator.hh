/* -*- mode:C++; indent-tabs-mode:nil; -*- */
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
#pragma once


#include "cpu/CoreLocal.hh"
#include "boot/mlog.hh"
#include "objects/ISchedulable.hh"

namespace mythos {

class SchedulingCoordinator;
extern CoreLocal<SchedulingCoordinator*> localCoordinator_ KERNEL_CLM_HOT;

/**
 * Class coordinates between Place(kernel task scheduler), SchedulingContext(system thread scheduler),
 * ExecutionContext (system thread) and the decision to go to sleep. Different policies of sleeping can be chosen.
 */
class SchedulingCoordinator {

  enum Policy {
    SLEEP = 0,
    SPIN  = 1,
  };

public:
  NORETURN void runUser() {
    switch (policy) {
      case SLEEP : runSleep();
      case SPIN  : runSpin();
      default    : runSleep();
    }
  }

  NORETURN void sleep() {
    MLOG_DETAIL(mlog::boot, "going to sleep now");
    mythos::idle::sleep(); // resets the kernel stack!
  }

  NORETURN void runSleep();
  NORETURN void runSpin();

  void init(mythos::async::Place *p, mythos::SchedulingContext *sc) {
    localPlace = p;
    localSchedulingContext = sc;
  }

  void setPolicy(Policy p) { policy = p; }

private:
  mythos::async::Place *localPlace = nullptr;
  mythos::SchedulingContext *localSchedulingContext = nullptr;

  Policy policy = {SLEEP};
};

} // namespace mythos
