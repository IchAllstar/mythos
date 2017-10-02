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
#include "app/SignalGroup.hh"
#include "util/Time.hh"
#include "app/HelperThreadBenchmark.hh"
#include "app/TreeMulticastBenchmark.hh"
#include "app/SequentialMulticastBenchmark.hh"
#include "runtime/HWThread.hh"
#include "app/TreeCombining.hh"

mythos::InvocationBuf* msg_ptr asm("msg_ptr");
int main() asm("main");

constexpr uint64_t stacksize = 8 * 4096;
char initstack[stacksize];
char* initstack_top = initstack + stacksize;

mythos::Portal portal(mythos::init::PORTAL, msg_ptr);
mythos::CapMap myCS(mythos::init::CSPACE);
mythos::PageMap myAS(mythos::init::PML4);
mythos::KernelMemory kmem(mythos::init::KM);
mythos::SimpleCapAllocDel capAlloc(portal, myCS, mythos::init::APP_CAP_START,
    mythos::init::SIZE - mythos::init::APP_CAP_START);

std::atomic<uint64_t> counter {0};
ThreadManager manager(portal, myCS, myAS, kmem, capAlloc);
mythos::TreeCombining<NUM_THREADS, 4> tc;

namespace mythos {
  uint64_t tscdelay_MHz = 1000;
}

uint64_t portalCap = 0;
void *ib = nullptr;
void* thread_main(void* data) {
  while(true) {
    mythos::syscall_wait();
    counter.fetch_add(1);
  }
}

char threadstack[stacksize];
char* threadstack_top = threadstack + stacksize;

uint64_t values[REPETITIONS] {0};
void test_raw() {

  mythos::ExecutionContext ec2(capAlloc());

  {
    mythos::PortalLock pl(portal);
    MLOG_INFO(mlog::app, "test_EC: create ec2");
    auto res2 = ec2.create(pl, kmem, myAS, myCS, mythos::init::SCHEDULERS_START+10,
        threadstack_top, &thread_main, nullptr).wait();
    TEST(res2);
  }

  mythos::Timer t;
  uint64_t sum = 0;
  for (int i = 0; i < REPETITIONS; i++) {
    counter.store(0);
    t.start();
    mythos::syscall_signal(ec2.cap());
    while(counter.load() == 0) {}
    auto val = t.end();
    sum += val;
    values[i] = val;
    mythos::hwthread_pause(1000);
  }
  for (auto val : values) {
    MLOG_ERROR(mlog::app, val);
  }
  MLOG_ERROR(mlog::app, "Avg:", sum/REPETITIONS);
}

void test_invocation_latency() {
  manager.init([](void *data) -> void*{
  });
  manager.startAll();
  mythos::Timer t;
  mythos::PortalLock pl(portal);
  mythos::HWThread hwt(mythos::init::HWTHREAD_START + 10);
  for (uint64_t i = 0; i < REPETITIONS; i++) {
    t.start();
    hwt.readSleepState(pl).wait();
    MLOG_ERROR(mlog::app, t.end());
  }
}

int main()
{
  MLOG_ERROR(mlog::app, "START application");

  //HelperThreadBenchmark htb(portal);
  //htb.test_multicast();

  TreeMulticastBenchmark tmb(portal);
  tmb.test_multicast();

  //SequentialMulticastBenchmark smb(portal);
  //smb.test_multicast();

  //test_raw();
  //test_invocation_latency();

  MLOG_ERROR(mlog::app, "END application");
  return 0;
}
