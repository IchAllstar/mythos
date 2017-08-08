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
#include "runtime/SignalableGroup.hh"
#include "app/mlog.hh"
#include <cstdint>
#include "util/optional.hh"
#include "util/Time.hh"
#include "app/Thread.hh"
#include "app/ThreadManager.hh"
#include "app/TreeMulticastBenchmark.hh"
#include "runtime/HelperThreadManager.hh"

mythos::InvocationBuf* msg_ptr asm("msg_ptr");
int main() asm("main");

constexpr uint64_t stacksize = 4 * 4096;
char initstack[stacksize];
char* initstack_top = initstack + stacksize;

mythos::Portal portal(mythos::init::PORTAL, msg_ptr);
mythos::CapMap cs(mythos::init::CSPACE);
mythos::PageMap as(mythos::init::PML4);
mythos::KernelMemory kmem(mythos::init::KM);
mythos::SimpleCapAllocDel caps(portal, cs, mythos::init::APP_CAP_START,
                               mythos::init::SIZE - mythos::init::APP_CAP_START);

char threadstack[stacksize];
char* thread1stack_top = threadstack + stacksize / 2;
char* thread2stack_top = threadstack + stacksize;

char threadstack2[stacksize];
char* thread3stack_top = threadstack2 + stacksize / 2;
char* thread4stack_top = threadstack2 + stacksize;

//uint64_t NUM_THREADS = 60;
//const uint64_t PAGE_SIZE  = 2 * 1024 * 1024; // 2 MB
//const uint64_t STACK_SIZE = 1 * PAGE_SIZE;

std::atomic<uint64_t> counter {0};

ThreadManager manager(portal, cs, as, kmem, caps);

void* thread_main(void* ctx)
{
  while (true) {
    MLOG_ERROR(mlog::app, "hello thread!", DVAR(ctx));
    mythos::ISysretHandler::handle(mythos::syscall_wait());
    counter.fetch_add(1);
    MLOG_ERROR(mlog::app, "thread resumed from wait", DVAR(ctx));
  }
  return 0;
}
/*
void init_threads() {
  mythos::PortalLock pl(portal);
  mythos::Frame stackFrame(caps.alloc());
  auto res1 = stackFrame.create(pl, kmem, NUM_THREADS * STACK_SIZE, PAGE_SIZE).wait();
  ASSERT(res1);
  MLOG_INFO(mlog::app, "alloc thread stack frame", DVAR(res1.state()));
  constexpr uintptr_t VADDR = 12 * PAGE_SIZE;
  auto res2 = as.mmap(pl, stackFrame, VADDR, NUM_THREADS * STACK_SIZE,
                      mythos::protocol::PageMap::MapFlags().writable(true)).wait();
  ASSERT(res2);
  MLOG_INFO(mlog::app, "mmap stack frame", DVAR(res2.state()),
            DVARhex(res2->vaddr), DVARhex(res2->size), DVAR(res2->level));

  mythos::SignalableGroup group(caps());
  TEST(group.create(pl, kmem, 100).wait());

  auto addr = reinterpret_cast<uint8_t*>(VADDR);
  for (uint64_t i = 6; i < NUM_THREADS; ++i) {
    addr += STACK_SIZE;
    mythos::ExecutionContext ec(caps());
    auto res2 = ec.create(pl, kmem, as, cs, mythos::init::SCHEDULERS_START + i,
                          addr, &thread_main, (void*)i).wait();
    TEST(res2);
    TEST(group.addMember(pl, ec.cap()).wait());
  }
  uint64_t start, end;
  start = mythos::getTime();
  //while(true) {
  group.signalAll(pl).wait();
  //}
  uint64_t old_counter = counter.load();
  while (counter.load() < NUM_THREADS - 1) {
    mythos::hwthread_pause();
    if (old_counter != counter.load()) {
      old_counter = counter.load();
      MLOG_ERROR(mlog::app, DVAR(counter.load()));
    }
  }
  end = mythos::getTime();
  MLOG_ERROR(mlog::app, DVAR(end - start), DVAR(counter.load()));
}

void test_signalable_group() {
  MLOG_ERROR(mlog::app, "begin test signalable group");
  mythos::PortalLock pl(portal);
  mythos::SignalableGroup group(caps());
  auto res1 = group.create(pl, kmem, 5);
  TEST(res1);

  mythos::ExecutionContext ec1(caps());
  auto res2 = ec1.create(pl, kmem, as, cs, mythos::init::SCHEDULERS_START + 2,
                         thread1stack_top, &thread_main, (void*)1).wait();
  TEST(res2);
  mythos::ExecutionContext ec2(caps());
  auto res3 = ec2.create(pl, kmem, as, cs, mythos::init::SCHEDULERS_START + 3,
                         thread2stack_top, &thread_main, (void*)2).wait();
  TEST(res3);
  mythos::ExecutionContext ec3(caps());
  auto res4 = ec3.create(pl, kmem, as, cs, mythos::init::SCHEDULERS_START + 4,
                         thread3stack_top, &thread_main, (void*)3).wait();
  TEST(res4);
  mythos::ExecutionContext ec4(caps());
  auto res5 = ec4.create(pl, kmem, as, cs, mythos::init::SCHEDULERS_START + 1,
                         thread4stack_top, &thread_main, (void*)4).wait();
  TEST(res5);
  TEST(group.addMember(pl, ec1.cap()).wait());
  TEST(group.addMember(pl, ec2.cap()).wait());
  TEST(group.addMember(pl, ec3.cap()).wait());
  TEST(group.addMember(pl, ec4.cap()).wait());

  TEST(group.signalAll(pl));
  mythos::ExecutionContext ec(caps());
  auto res6 = ec.create(pl, kmem, as, cs, mythos::init::SCHEDULERS_START + 30,
                         thread4stack_top, &thread_main, (void*)4).wait();
  TEST(res6);
  MLOG_ERROR(mlog::app, "end test signalable group");
}
*/
int main()
{
  mythos::PortalLock pl(portal);
  mythos::HelperThreadManager hpm(mythos::init::HELPER_THREAD_MANAGER);

  hpm.registerHelper(pl, 63).wait();
  pl.release();
  //TreeMulticastBenchmark tmb(portal);
  //tmb.test_multicast();
  /*
  char const end[] = "Application exited.";
  MLOG_ERROR(mlog::app, "Application started.");

  test_signalable_group();
  //init_threads();

  mythos::syscall_debug(end, sizeof(end) - 1);
  */
  return 0;
}
