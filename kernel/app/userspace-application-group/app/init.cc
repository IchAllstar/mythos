/* -*- mode:C++; indent-tabs-mode:nil; -*- */
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

#include "mythos/init.hh"
#include "mythos/invocation.hh"
#include "mythos/protocol/CpuDriverKNC.hh"
#include "mythos/PciMsgQueueMPSC.hh"
#include "runtime/Portal.hh"
#include "runtime/ExecutionContext.hh"
#include "runtime/CapMap.hh"
#include "runtime/Example.hh"
#include "runtime/InterruptControl.hh"
#include "runtime/PageMap.hh"
#include "runtime/KernelMemory.hh"
#include "runtime/SimpleCapAlloc.hh"
#include "app/mlog.hh"
#include <cstdint>
#include "app/Thread.hh"
#include "app/Multicast.hh"
#include "util/Time.hh"

mythos::InvocationBuf* msg_ptr asm("msg_ptr");
int main() asm("main");

constexpr uint64_t stacksize = 4*4096;
char initstack[stacksize];
char* initstack_top = initstack+stacksize;

// [ ) range
struct ThreadRange {
  std::atomic<bool> onGoing {false};
  ISignalable **group {nullptr};
  uint64_t from = 0;
  uint64_t to   = 0;
};

mythos::Portal portal(mythos::init::PORTAL, msg_ptr);
mythos::CapMap myCS(mythos::init::CSPACE);
mythos::PageMap myAS(mythos::init::PML4);
mythos::KernelMemory kmem(mythos::init::KM);
mythos::SimpleCapAllocDel capAlloc(portal, myCS, mythos::init::APP_CAP_START,
                                  mythos::init::SIZE-mythos::init::APP_CAP_START);

using mythos::getTime;
std::atomic<uint64_t> counter {0};
extern const size_t NUM_THREADS;
extern const size_t NUM_HELPER_THREADS;

ThreadManager manager(portal, myCS, myAS, kmem, capAlloc);
ThreadRange helper_info[NUM_HELPER_THREADS];

void* helper_thread(void *data) {
  ASSERT(data != nullptr);
  auto *thread = reinterpret_cast<Thread*>(data);
  MLOG_ERROR(mlog::app, "Helper started", DVAR(thread->id));
  while(true)  {
    auto prev = thread->SIGNALLED.exchange(false);
    if (prev) {
      MLOG_ERROR(mlog::app, "Got Signalled.");
      auto &info = helper_info[thread->id];
      if (info.onGoing.load()) {
        //do stuff
        for (uint64_t i = info.from; i < info.to; i++) {
          MLOG_ERROR(mlog::app, "Signal", i);
          info.group[i]->signal();
        }
        info.onGoing.store(false);
      }
    }
    Thread::wait(*thread);
  }
}

void test_helper() {
  SignalableGroup<100, HelperStrategy> group;
  for (int i = NUM_HELPER_THREADS; i < manager.getNumThreads(); ++i) {
    group.addMember(manager.getThread(i));
  }
  //group.signalAll();
}

int main()
{
  manager.init([](void *data) -> void* {
      ASSERT(data != nullptr);
      auto *thread = reinterpret_cast<Thread*>(data);
      if (manager.isHelper(*thread)) {
        return helper_thread(data);
      } else {
        counter.fetch_add(1);
      }
  });
  for (uint64_t i = 1; i < NUM_HELPER_THREADS; ++i) {
    manager.registerHelper(*manager.getThread(i));
  }
  manager.startAll();

  if (NUM_HELPER_THREADS > 0) {
    test_helper();
    return 0;
  }

  // wait until all initialized
  while (counter.load() < NUM_THREADS - 1) {
    mythos::hwthread_pause(100);
  };

  MLOG_ERROR(mlog::app, "Signalable Group Test");
  SignalableGroup<100> group;
  //SignalableGroup<100, TreeStrategy> group;
  for (int i = 1; i < manager.getNumThreads(); ++i) {
    group.addMember(manager.getThread(i));
  }

  counter.store(0);

  uint64_t start ,end;
  start = getTime();
  group.signalAll();
  while (counter.load() < NUM_THREADS - 1) {
    mythos::hwthread_pause(100);
  }

  end = getTime();
  MLOG_ERROR(mlog::app, DVAR(end - start));
  return 0;
}
