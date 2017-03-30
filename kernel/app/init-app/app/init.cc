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

#include "mythos/init.hh"
#include "mythos/invocation.hh"
#include "runtime/Portal.hh"
#include "runtime/ExecutionContext.hh"
#include "runtime/CapMap.hh"
#include "runtime/Example.hh"
#include "runtime/PageMap.hh"
#include "runtime/UntypedMemory.hh"
#include "runtime/SimpleCapAlloc.hh"
#include "app/mlog.hh"
#include <cstdint>
#include "util/optional.hh"

mythos::InvocationBuf* msg_ptr asm("msg_ptr");
int main() asm("main");


constexpr uint64_t MAX_NUMBER_HWT = 200;
constexpr uint64_t STACKSIZE = 4*4096;

// init stack
char initstack[STACKSIZE];
char* initstack_top = initstack+STACKSIZE;

// benchmark thread stack
char thread_stack[STACKSIZE * MAX_NUMBER_HWT];
char *thread_stack_top = thread_stack + STACKSIZE * MAX_NUMBER_HWT;


mythos::Portal portal(mythos::init::PORTAL, msg_ptr);
mythos::CapMap myCS(mythos::init::CSPACE);
mythos::PageMap myAS(mythos::init::PML4);
mythos::UntypedMemory kmem(mythos::init::UM);

char threadstack[STACKSIZE];
char* thread1stack_top = threadstack+STACKSIZE;
char* thread2stack_top = threadstack+STACKSIZE / 2;

uint64_t get_time(){
  uint64_t hi, lo;
  asm volatile("rdtsc":"=a"(lo), "=d"(hi));
  return ((uint64_t)lo)|( ((uint64_t)hi)<<32);
}

void* thread_main(void* ctx)
{
  uint64_t id = (uint64_t) ctx;
  MLOG_ERROR(mlog::app, "hello thread wait!", DVAR(id));
  //mythos::ISysretHandler::handle(mythos::syscall_wait());
  //MLOG_ERROR(mlog::app, "thread resumed from wait", DVAR(ctx), DVAR(get_time()));
  return 0;
}

void create_run_n_threads(uint64_t threads){
  mythos::PortalFutureRef<void> res1 = mythos::PortalFutureRef<void>(portal);
  for (uint64_t i = 0; i < threads; i++) {

    mythos::ExecutionContext ec(mythos::init::APP_CAP_START + 1 + i);
    res1 = ec.create(res1.reuse(), kmem, mythos::init::EXECUTION_CONTEXT_FACTORY,
                         myAS, myCS, mythos::init::SCHEDULERS_START + 1 + i,
                         thread_stack_top - STACKSIZE * i, &thread_main, (void*)i);
    res1.wait();
    ASSERT(res1.state() == mythos::Error::SUCCESS);
    MLOG_ERROR(mlog::app, "Created Thread", i);
  }
}


void benchmark_wakeup_ecs() {
  create_run_n_threads(100);
}


int main()
{
  MLOG_ERROR(mlog::app, "Start time", DVAR(get_time()));
  benchmark_wakeup_ecs();
/*
  for (int run=1; run<=100; run++) {
    MLOG_ERROR(mlog::app, "beginning test run", run);
    test_Example();
    test_Portal();
  }
  
  mythos::ExecutionContext ec1(mythos::init::APP_CAP_START);
  auto res1 = ec1.create(portal, kmem, mythos::init::EXECUTION_CONTEXT_FACTORY,
                         myAS, myCS, mythos::init::SCHEDULERS_START,
                         thread1stack_top, &thread_main, nullptr);
  res1.wait();
  ASSERT(res1.state() == mythos::Error::SUCCESS);

  mythos::ExecutionContext ec2(mythos::init::APP_CAP_START+1);
  auto res2 = ec2.create(res1.reuse(), kmem, mythos::init::EXECUTION_CONTEXT_FACTORY,
                         myAS, myCS, mythos::init::SCHEDULERS_START+1,
                         thread2stack_top, &thread_main, nullptr);
  res2.wait();
  ASSERT(res2.state() == mythos::Error::SUCCESS);

  MLOG_ERROR(mlog::app, "notify");
  mythos::ISysretHandler::handle(mythos::syscall_notify(ec1.cap()));
  mythos::ISysretHandler::handle(mythos::syscall_notify(ec2.cap()));
*/
  MLOG_ERROR(mlog::app, "Finished time", DVAR(get_time()));

  for (volatile int i=0; i<100000; i++) {
    for (volatile int j=0; j<1000; j++) {}
  }
  return 0;
}
