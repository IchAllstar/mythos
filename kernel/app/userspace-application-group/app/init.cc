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

mythos::Portal portal(mythos::init::PORTAL, msg_ptr);
mythos::CapMap myCS(mythos::init::CSPACE);
mythos::PageMap myAS(mythos::init::PML4);
mythos::KernelMemory kmem(mythos::init::KM);
mythos::SimpleCapAllocDel capAlloc(portal, myCS, mythos::init::APP_CAP_START,
                                  mythos::init::SIZE-mythos::init::APP_CAP_START);

using mythos::getTime;
std::atomic<uint64_t> counter {0};

ThreadManager manager(portal, myCS, myAS, kmem, capAlloc);
extern bool signaled[100];
int main()
{

  manager.init([](void *data) -> void* {
    //MLOG_ERROR(mlog::app, "Hello Thread");
    //MLOG_ERROR(mlog::app, "Do work");
    counter.fetch_add(1);
    /*auto *thread = reinterpret_cast<Thread*>(data);
    if (thread->id == 1) {
      thread->signal(*manager.getThread(2));
    } else {
      thread->signal(*manager.getThread(1));
    }
    thread->wait(*thread);
    */
  });
  manager.startAll();

  // wait until all initialized
  while (counter.load() < 99) {
  };

  MLOG_ERROR(mlog::app, "Signalable Group Test");
  SignalableGroup<TreeStrategy> group;
  for (int i = 1; i < manager.getNumThreads(); ++i) {
    group.addMember(manager.getThread(i));
  }

  counter.store(0);

  uint64_t start ,end;
  start = getTime();
  group.signalAll((void*)5);
  while (counter.load() < 99) {
    //MLOG_ERROR(mlog::app, DVAR(counter.load()));
    /*char c[101];
    c[100] = 0;
    for (int i = 0; i < 100; i++) {
      if (signaled[i]) {
        c[i] = '1';
      } else {
        c[i] = '0';
      }
    }
    MLOG_ERROR(mlog::app, DVAR(c));
  */
  }

  end = getTime();
  MLOG_ERROR(mlog::app, DVAR(end - start));
  //group.signalAll((void*)5);
  //group.signalAll((void*)5);
  return 0;
}
