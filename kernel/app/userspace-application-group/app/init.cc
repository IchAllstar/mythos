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
#include "app/ThreadManager.hh"
#include "app/SignalableGroup.hh"
#include "util/Time.hh"
#include "app/HelperThread.hh"

mythos::InvocationBuf* msg_ptr asm("msg_ptr");
int main() asm("main");

constexpr uint64_t stacksize = 4*4096;
char initstack[stacksize];
char* initstack_top = initstack+stacksize;

mythos::Portal portal(mythos::init::PORTAL, msg_ptr);
mythos::CapMap myCS(mythos::init::CSPACE);
mythos::PageMap myAS(mythos::init::PML4);
mythos::KernelMemory kmem(mythos::init::KM);
mythos::SimpleCapAllocDel capAlloc(portal, myCS, mythos::init::APP_CAP_START,
                                  mythos::init::SIZE-mythos::init::APP_CAP_START);

using mythos::getTime;
std::atomic<uint64_t> counter {0};
extern const size_t NUM_THREADS;
extern const size_t NUM_HELPER;

ThreadManager manager(portal, myCS, myAS, kmem, capAlloc);


HelperThread helpers[NUM_HELPER];
// Place the helper on following threads
uint64_t helperIDs[NUM_HELPER] = {1,3,4,7,8,11,25,29,33,37};

HelperThread *getHelper(uint64_t threadID) {
  for (auto &h : helpers) {
    if (h.thread->id == threadID) {
      return &h;
    }
  }
  return nullptr;
}

bool contains(uint64_t id) {
  for (auto i : helperIDs) {
    if (id == i) return true;
  }
  return false;
}

void* helper_thread(void *data) {
  ASSERT(data != nullptr);
  auto *thread = reinterpret_cast<Thread*>(data);
  //MLOG_ERROR(mlog::app, "Helper started", DVAR(thread->id));
  auto *helper = getHelper(thread->id);
  ASSERT(helper);
  while(true)  {
    auto prev = thread->SIGNALLED.exchange(false);
    if (prev) {
      //MLOG_ERROR(mlog::app, "Helper: Got Signalled.");
      //do useful stuff
      uint64_t start , end;
      start = getTime();
      for (uint64_t i = helper->from; i < helper->to; ++i) {
        //MLOG_ERROR(mlog::app, "wakeup", i);
        helper->group[i]->signal();
      }
      helper->onGoing.store(false);
      end = getTime();
      //MLOG_ERROR(mlog::app, DVAR(end - start));
    }
    Thread::wait(*thread);
  }
}

void init_helper() {
  uint64_t helper_number = 0;
  for (auto i : helperIDs/*uint64_t i = 1; i < NUM_HELPER + 1; ++i*/) {
    manager.initThread(i, &helper_thread);
    ASSERT(manager.getThread(i));
    helpers[helper_number].thread = manager.getThread(i);
    manager.startThread(*manager.getThread(i));
    helper_number++;
  }
}

void init_worker() {
  for (uint64_t i = 1; i < NUM_THREADS; i++) {
    if (contains(i)) continue;

    manager.initThread(i, [](void *data) -> void* {
      auto *thread = reinterpret_cast<Thread*>(data);
      counter.fetch_add(1);
    });
    manager.startThread(*manager.getThread(i));
  }
}

void test_helper() {
  init_helper();
  init_worker();
  SignalableGroup<100, HelperStrategy> group;
  for (int i = 1; i < NUM_THREADS; i++) {
    if (contains(i)) continue;
    group.addMember(manager.getThread(i));
  }
  counter.store(0);
  uint64_t start,middle,end;
  start = getTime();
  group.signalAll();
  middle = getTime();
  while (counter.load(std::memory_order_relaxed) < NUM_THREADS - NUM_HELPER - 1) {
    mythos::hwthread_pause();
  }
  end = getTime();
  MLOG_ERROR(mlog::app, "Num Threads:", group.count(), "Helper:", NUM_HELPER, "Time:", end - start, "Middle", middle - start);
}


void test_multicast() {
  manager.init([](void *data) -> void* {
      ASSERT(data != nullptr);
      auto *thread = reinterpret_cast<Thread*>(data);
      counter.fetch_add(1);
  });
  manager.startAll();

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
  MLOG_ERROR(mlog::app,DVAR(group.count()),  DVAR(end - start));
}

int main()
{
  MLOG_ERROR(mlog::app, "START application");
  //test_multicast();
  test_helper();
  MLOG_ERROR(mlog::app, "END application");
  return 0;
}
