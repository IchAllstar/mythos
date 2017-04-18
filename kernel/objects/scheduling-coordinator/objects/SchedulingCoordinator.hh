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
//#include "async/NestedMonitorHome.hh"
#include "objects/ISchedulable.hh"
#include "objects/IKernelObject.hh"
#include "objects/IFactory.hh"
#include "mythos/protocol/KernelObject.hh"
#include "mythos/protocol/SchedulingCoordinator.hh"
#include "boot/mlog.hh"

namespace mythos {

class SchedulingCoordinator;
extern CoreLocal<SchedulingCoordinator*> localCoordinator_ KERNEL_CLM_HOT;

/**
 * Class coordinates between Place(kernel task scheduler), SchedulingContext(system thread scheduler),
 * ExecutionContext (system thread) and the decision to go to sleep. Different policies of sleeping can be chosen.
 */
class SchedulingCoordinator
  : public IKernelObject {

  enum Policy {
    SLEEP = 0,
    SPIN  = 1,
  };

//IKernelObject interface
public:
  optional<void> deleteCap(Cap self, IDeleter& del) override;
  void invoke(Tasklet* t, Cap self, IInvocation* msg) override;
  optional<void const*> vcast(TypeId id) const override;

//protocol implementations
protected:
  friend struct protocol::SchedulingCoordinator;
  Error printMessage(Tasklet *t, Cap self, IInvocation *msg);
  Error setPolicy(Tasklet *t, Cap self, IInvocation *msg);

  friend struct protocol::KernelObject;
  Error getDebugInfo(Cap self, IInvocation* msg);


// Actual Methods
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
    mythos::cpu::go_sleeping(); // resets the kernel stack!
  }

  NORETURN void runSleep();
  NORETURN void runSpin();

  void init(mythos::async::Place *p, mythos::SchedulingContext *sc) {
    ASSERT(p != nullptr);
    ASSERT(sc != nullptr);
    localPlace = p;
    localSchedulingContext = sc;
    monitor.setHome(p);
  }

private:
  // kernel object stuff
  IDeleter::handle_t del_handle = {this};
  async::NestedMonitorHome monitor;

  // actual stuff
  mythos::async::Place *localPlace = nullptr;
  mythos::SchedulingContext *localSchedulingContext = nullptr;

  Policy policy = {SLEEP};
};

} // namespace mythos
