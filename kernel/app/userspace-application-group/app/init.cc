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

namespace mythos {
  uint64_t tscdelay_MHz = 1000;
}

uint64_t portalCap = 0;
void *ib = nullptr;
void* thread_main(void* data) {
  mythos::syscall_wait();
  mythos::Portal p(portalCap, ib);
  mythos::PortalLock pl(p);
  for (auto a : {0,2,5,7,9,24,56,89}) {
    mythos::HWThread hw(mythos::init::HWTHREAD_START + a);
    auto res = hw.readSleepState(pl).wait();
    MLOG_ERROR(mlog::app, res->sleep_state);
  }
  pl.release();
  MLOG_ERROR(mlog::app, "Test passed");
  while(true) {
    mythos::syscall_wait();
    counter.fetch_add(1);
  }
}

void createPortalForEC(mythos::ExecutionContext ec) {
  MLOG_ERROR(mlog::app, "test_Portal begin");
  mythos::PortalLock pl(portal); // future access will fail if the portal is in use already
  MLOG_INFO(mlog::app, "test_Portal: allocate portal");
  uintptr_t vaddr = 22*1024*1024; // choose address different from invokation buffer
  // allocate a portal
  mythos::Portal p2(capAlloc(), (void*)vaddr);
  auto res1 = p2.create(pl, kmem).wait();
  TEST(res1);
  // allocate a 2MiB frame
  MLOG_INFO(mlog::app, "test_Portal: allocate frame");
  mythos::Frame f(capAlloc());
  auto res2 = f.create(pl, kmem, 2*1024*1024, 2*1024*1024).wait();
  MLOG_INFO(mlog::app, "alloc frame",DVAR(res2.state()));
  TEST(res2);
  // map the frame into our address space
  MLOG_INFO(mlog::app, "test_Portal: mapframe");
  auto res3 = myAS.mmap(pl, f, vaddr, 2*1024*1024, 0x1).wait();
  MLOG_ERROR(mlog::app, "mmap frame",DVAR(res3.state()),DVARhex(res3->vaddr),DVARhex(res3->size),DVAR(res3->level));
  TEST(res3);
  // bind the portal in order to receive responses
  MLOG_INFO(mlog::app,"test_Portal: configure portal");
  auto res4 = p2.bind(pl, f, 0, ec.cap()).wait();
  TEST(res4);
  ib = (void*)vaddr;
  portalCap = p2.cap();
}

char threadstack[stacksize];
char* threadstack_top = threadstack + stacksize;
void test_raw() {

  mythos::ExecutionContext ec2(capAlloc());

  {
    mythos::PortalLock pl(portal);
    MLOG_INFO(mlog::app, "test_EC: create ec2");
    auto res2 = ec2.create(pl, kmem, myAS, myCS, mythos::init::SCHEDULERS_START+10,
        threadstack_top, &thread_main, nullptr).wait();
    TEST(res2);
  }

  createPortalForEC(ec2.cap());
  mythos::syscall_signal(ec2.cap());
  mythos::hwthread_wait(1000);

  mythos::Timer t;
  uint64_t sum = 0;
  for (int i = 0; i < REPETITIONS; i++) {
    counter.store(0);
    t.start();
    mythos::syscall_signal(ec2.cap());
    while(counter.load() == 0) {}
    sum += t.end();
    mythos::hwthread_pause(10000);
  }
  MLOG_ERROR(mlog::app, DVAR(sum/REPETITIONS));
}

void test_portal() {
  manager.init([](void *data) -> void*{
    auto *thread = (Thread*) data;
    Thread::wait(*thread);
    mythos::Portal p(thread->portal, thread->ib);
    mythos::PortalLock pl(p);
    mythos::HWThread hwt(mythos::init::HWTHREAD_START + 20);
    auto res = hwt.readSleepState(pl).wait();
    if (res)
      MLOG_ERROR(mlog::app, res->sleep_state);
  });
  manager.startAll();
  for (int i = 0; i < manager.getNumThreads(); i++) {
    manager.getThread(i)->signal();
  }
  for (int i = 0; i < manager.getNumThreads(); i++) {
    manager.getThread(i)->signal();
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

  //test_portal();

  MLOG_ERROR(mlog::app, "END application");
  return 0;
}
