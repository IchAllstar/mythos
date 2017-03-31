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
#include "app/mlog.hh"
#include <cstdint>
#include "util/optional.hh"

#define NUM_RUNS 1000
#define PARALLEL_ECS 200
#define AVAILABLE_HWTS 200

mythos::InvocationBuf* msg_ptr asm("msg_ptr");
int main() asm("main");

constexpr uint64_t ONE_STACK = 4096 * 4;
constexpr uint64_t STACKSIZE = AVAILABLE_HWTS*ONE_STACK;
// needed or program crashes with various errors because no stack for the main()
char initstack[ONE_STACK];
char* initstack_top = initstack+ONE_STACK;

mythos::Portal myPortal(mythos::init::PORTAL, msg_ptr);
mythos::CapMap myCS(mythos::init::CSPACE);
mythos::PageMap myAS(mythos::init::PML4);
mythos::UntypedMemory kmem(mythos::init::UM);

char threadstack[STACKSIZE];
char* threadstack_top = threadstack+STACKSIZE;

uint64_t getTime(){
	uint64_t hi, lo;
	asm volatile("rdtsc":"=a"(lo), "=d"(hi));
	return ((uint64_t)lo)|( ((uint64_t)hi)<<32);
}

void* thread_main(void* ctx){
  // MLOG_ERROR(mlog::app, "Hello from Thread ", (uint64_t)ctx);
  return 0;
}

void simple_init_ecs(uint64_t number, mythos::PortalFutureRef<void> res1) {
  for (uint64_t i = 0; i < number; i++) {
		mythos::ExecutionContext ec(mythos::init::APP_CAP_START+1+i);
		res1 = ec.create(res1.reuse(), kmem, mythos::init::EXECUTION_CONTEXT_FACTORY,
				myAS, myCS, mythos::init::SCHEDULERS_START+1+i,
				threadstack_top-ONE_STACK*i, &thread_main, (void*)i);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);
    //MLOG_ERROR(mlog::app, "Created Thread ", i);
  }
}

uint64_t simple_run_ecs(uint64_t number, mythos::PortalFutureRef<void> res1) {
  uint64_t start, end;
  start = getTime();
  for (uint64_t i = 0; i < number; i++) {
    mythos::ExecutionContext ec(mythos::init::APP_CAP_START+1+i);
		res1 = ec.run(res1.reuse());
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);
    //MLOG_ERROR(mlog::app, "Running thread ", i);
  }
  end = getTime();
  return end - start;
}

void benchmark_wakeup_hwts() {
  uint64_t time = 0;
  mythos::PortalFutureRef<void> res1 = mythos::PortalFutureRef<void>(myPortal);
  simple_init_ecs(100, res1);
  time = simple_run_ecs(100, res1);
  MLOG_ERROR(mlog::app,"HWT:",100, " took cycles: ", time);
}

void benchmarks(){
  benchmark_wakeup_hwts();
}

int main()
{
	char const begin[] = "Starting the benchmarks";
	char const end[] = "Benchmarks are done";
	mythos::syscall_debug(begin, sizeof(begin)-1);

	benchmarks();

	mythos::syscall_debug(end, sizeof(end)-1);

  for (volatile uint64_t i = 0; i < 10000; i++) {
    for (volatile uint64_t ii = 0; ii < 100000; ii++) {
    }
  }

	return 0;
}
