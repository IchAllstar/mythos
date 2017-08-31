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
#include "runtime/SignalGroup.hh"
#include "app/mlog.hh"
#include <cstdint>
#include "util/optional.hh"
#include "util/Time.hh"

mythos::InvocationBuf* msg_ptr asm("msg_ptr");
int main() asm("main");

constexpr uint64_t stacksize = 4 * 4096;
char initstack[stacksize];
char* initstack_top = initstack + stacksize;

mythos::Portal portal(mythos::init::PORTAL, msg_ptr);
mythos::CapMap myCS(mythos::init::CSPACE);
mythos::PageMap myAS(mythos::init::PML4);
mythos::KernelMemory kmem(mythos::init::KM);
mythos::SimpleCapAllocDel capAlloc(portal, myCS, mythos::init::APP_CAP_START,
                                   mythos::init::SIZE - mythos::init::APP_CAP_START);

char threadstack[stacksize];
char* thread1stack_top = threadstack + stacksize / 2;
char* thread2stack_top = threadstack + stacksize;

char threadstack2[stacksize];
char* thread3stack_top = threadstack2 + stacksize / 2;
char* thread4stack_top = threadstack2 + stacksize;

uint64_t REPETITIONS = 1000;
std::atomic<uint64_t> counter = {0};

void* thread_main(void* ctx)
{
  MLOG_ERROR(mlog::app, "hello thread!", DVAR(ctx));
  while(true) {
    mythos::ISysretHandler::handle(mythos::syscall_wait());
    counter.fetch_add(1);
    //MLOG_ERROR(mlog::app, "thread resumed from wait", DVAR(ctx));
  }
  return 0;
}

int main()
{
  char const str[] = "hello world!";
  char const end[] = "bye, cruel world!";
  mythos::syscall_debug(str, sizeof(str) - 1);
  MLOG_ERROR(mlog::app, "application is starting :)", DVARhex(msg_ptr), DVARhex(initstack_top));


  mythos::ExecutionContext ec1(capAlloc());
  mythos::ExecutionContext ec2(capAlloc());
  mythos::ExecutionContext ec3(capAlloc());
  mythos::ExecutionContext ec4(capAlloc());
  {
    MLOG_INFO(mlog::app, "test_EC: create ec1");
    mythos::PortalLock pl(portal); // future access will fail if the portal is in use already
    auto res1 = ec1.create(pl, kmem, myAS, myCS, mythos::init::SCHEDULERS_START,
                           thread1stack_top, &thread_main, nullptr).wait();
    TEST(res1);

    MLOG_INFO(mlog::app, "test_EC: create ec2");
    auto res2 = ec2.create(pl, kmem, myAS, myCS, mythos::init::SCHEDULERS_START + 1,
                           thread2stack_top, &thread_main, (void*)1).wait();
    TEST(res2);
    auto res3 = ec3.create(pl, kmem, myAS, myCS, mythos::init::SCHEDULERS_START + 2,
                           thread3stack_top, &thread_main, (void*)2).wait();
    TEST(res3);
    auto res4 = ec4.create(pl, kmem, myAS, myCS, mythos::init::SCHEDULERS_START + 3,
                           thread4stack_top, &thread_main, (void*)3).wait();
    TEST(res4);

  }

  mythos::Timer t;
  uint64_t sum = 0;
  for (uint64_t i = 0; i < 1000; i++) {
    counter.store(0);
    t.start();
    mythos::syscall_signal(ec2.cap());
    while(counter.load() == 0) {  }
    sum += t.end();
    mythos::hwthread_pause(1000);
  }
  MLOG_ERROR(mlog::app, "raw thread singaling", sum / REPETITIONS);


  mythos::syscall_debug(end, sizeof(end) - 1);

  return 0;
}
