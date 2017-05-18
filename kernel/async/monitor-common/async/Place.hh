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
 * Copyright 2016 Randolf Rotta, Robert Kuban, and contributors, BTU Cottbus-Senftenberg
 */
#pragma once

#include "util/assert.hh"
#include <atomic>
#include "cpu/hwthread_pause.hh"
#include "cpu/hwthreadid.hh"
#include "cpu/CoreLocal.hh"
#include "async/Tasklet.hh"
#include "async/mlog.hh"
#include "async/TaskletQueue.hh"
#include "cpu/LAPIC.hh"

namespace mythos {
namespace async {

class Place;
extern CoreLocal<Place*> localPlace_ KERNEL_CLM_HOT;
inline Place& getLocalPlace() { return *localPlace_; }

/** Local scheduling for a single thread. */
class Place
{

public:

  void init(size_t apicID);
  bool isLocal() const { return this == &getLocalPlace(); }

  void setCR3(PhysPtr<void> value);
  PhysPtr<void> getCR3();

  enum Mode { ASYNC, MAYINLINE };

  void runLocal(TaskletBase* msg, Mode mode=ASYNC) {
    ASSERT(isLocal());
    if (mode == ASYNC) {
      pushPrivate(msg);
    } else {
      //MLOG_DETAIL(mlog::async, this, "run tasklet", msg);
      msg->run();
    }
  }

  void run(TaskletBase* msg, Mode mode=ASYNC) {
    if (isLocal()) runLocal(msg, mode);
    else pushShared(msg);
  }

  void pushShared(TaskletBase* msg) {
    ASSERT(msg);
    MLOG_DETAIL(mlog::async, this, "push shared", msg);
    if (queue.push(*msg)) wakeup();
  }

  /** prepare the kernel's task processing. returns true if nested entry. */
  bool enterKernel() {
    // ensure that incoming messages do not send IPI if we entered through local system call
    queue.tryAcquire();
    return nestingMonitor.exchange(true); // relaxed?
  }

  /** release queue and kernel monitor until successfull */
  bool releaseKernel() {
    if (not queue.tryRelease()) {
      return false;
    }
    nestingMonitor.store(false); // release?
    return true;
  }

  /** Processes tasks until the queue is empty and the atomic
   * unlocking was successfull. Hence, the next sender will detect
   * that he has to wakeup this place.
   */
  int processTasks();

  bool tryProcess();

  bool isActive() const { return nestingMonitor.load(std::memory_order_relaxed); }

  void wakeup() { MLOG_ERROR(mlog::async, "wakeup", DVAR(apicID)); mythos::lapic.sendIRQ(apicID, 32); }

protected:
  void pushPrivate(TaskletBase* msg) {
    ASSERT(isLocal());
    ASSERT(msg);
    MLOG_DETAIL(mlog::async, this, "push private", msg);
    queue.pushPrivate(*msg);
  }

protected:
  size_t apicID; //< for wakeup signals
  std::atomic<bool> nestingMonitor;
  TaskletQueueImpl<ChainFIFOBaseAligned> queue; //< for pending tasks
  PhysPtr<void> _cr3;
  TaskletBase *peak {nullptr};
};

/// @todo Should be allocated into local cachelines.
/// @todo should be moved to the boot/deployment code instead of hardcoding here!
extern Place places[BOOT_MAX_THREADS];
inline Place* getPlace(size_t threadid) { return &places[threadid]; }

inline void preparePlace(size_t apicID) {
  places[apicID].init(apicID);
}

inline void initLocalPlace(size_t apicID) { localPlace_.set(&places[apicID]); }

} // namespace async
using async::getLocalPlace;
} // namespace mythos
