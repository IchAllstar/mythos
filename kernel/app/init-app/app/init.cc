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

#define NUM_RUNS 10
#define PARALLEL_ECS 4
#define AVAILABLE_HWTS 4

mythos::InvocationBuf* msg_ptr asm("msg_ptr");
int main() asm("main");

constexpr uint64_t stacksize = AVAILABLE_HWTS*4096;
char initstack[stacksize];
char* initstack_top = initstack+stacksize;

mythos::Portal myPortal(mythos::init::PORTAL, msg_ptr);
mythos::CapMap myCS(mythos::init::CSPACE);
mythos::PageMap myAS(mythos::init::PML4);
mythos::UntypedMemory kmem(mythos::init::UM);

char threadstack[stacksize];
char* thread1stack_top = threadstack+stacksize/2;
char* thread2stack_top = threadstack+stacksize;

uint64_t getTime(){
	uint64_t hi, lo;
	asm volatile("rdtsc":"=a"(lo), "=d"(hi));
	return ((uint64_t)lo)|( ((uint64_t)hi)<<32);
}

void thread_mobileKernelObjectLatency(mythos::PortalRef portal, mythos::InvocationBuf* ib, mythos::CapPtr exampleCap){
	mythos::Example example(exampleCap);

	mythos::PortalFutureRef<void> res1 = mythos::PortalFutureRef<void>(std::move(portal));

	//Invocation latency to mobile kernel object
	uint64_t start, mid, end;
	char str[] = "01234567890123456"; //Dummy String


	for (size_t i = 0; i < NUM_RUNS; i++){
		start = getTime();
		res1 = example.ping(res1.reuse(),1e6);
		mid = getTime();
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);
		end = getTime();
		if (end < start){
			i--;
			continue;
		}
		MLOG_ERROR(mlog::app, "object location:", ib->cast<mythos::protocol::Example::Ping>()->place);
		MLOG_ERROR(mlog::app, "duration until first return: ", mid - start);
		MLOG_ERROR(mlog::app, "duration until reply: ", end - start);
	}
}

void* thread_invocationLatencyMain(void* ctx)
{
	size_t ownid = (size_t)ctx;

	mythos::InvocationBuf* ib = (mythos::InvocationBuf*)(((11+ownid)<<21));
	mythos::Portal portal(mythos::init::APP_CAP_START+2+ownid*3, ib);

	thread_mobileKernelObjectLatency(portal, ib, mythos::init::APP_CAP_START);

	return 0;
}

void* thread_main(void* ctx){
	return 0;
}

void mobileKernelObjectLatency(){
	//Create a mobile kernel object
	mythos::Example example(mythos::init::APP_CAP_START);
	auto res1 = example.create(myPortal, kmem, mythos::init::EXAMPLE_FACTORY);
	res1.wait();
	ASSERT(res1.state() == mythos::Error::SUCCESS);

	res1 = example.ping(res1.reuse(),0);
	res1.wait();
	ASSERT(res1.state() == mythos::Error::SUCCESS);


	//Create (parallel_ecs - 1) additional ECs
	for (size_t worker = 0; worker < PARALLEL_ECS - 1; worker++){
		//Create a new invocation buffer
		mythos::Frame new_msg_ptr(mythos::init::APP_CAP_START+1+worker*3);
		res1 = new_msg_ptr.create(res1.reuse(), kmem, mythos::init::MEMORY_REGION_FACTORY, 1<<21, 1<<21);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);

		//Map the invocation buffer frame into user space memory
		mythos::InvocationBuf* ib = (mythos::InvocationBuf*)(((11+worker)<<21));

		auto res3 = myAS.mmap(res1.reuse(), new_msg_ptr, (uintptr_t)ib, 1<<21, mythos::protocol::PageMap::MapFlags().writable(true));
		res3.wait();
		ASSERT(res3.state() == mythos::Error::SUCCESS);

		//Create a second portal
		mythos::Portal portal2(mythos::init::APP_CAP_START+2+worker*3, ib);
		res1 = portal2.create(res3.reuse(), kmem, mythos::init::PORTAL_FACTORY);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);

		//Create a second EC on its own HWT
		mythos::ExecutionContext ec(mythos::init::APP_CAP_START+3+worker*3);
		res1 = ec.create(res1.reuse(), kmem, mythos::init::EXECUTION_CONTEXT_FACTORY,
				myAS, myCS, mythos::init::SCHEDULERS_START+1+worker,
				initstack_top+stacksize-4096*worker, &thread_invocationLatencyMain, (void*)worker);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);

		//Bind the new portal to the new EC
		res1 = portal2.bind(res1.reuse(), new_msg_ptr, 0, mythos::init::APP_CAP_START+3+worker*3);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);
	}

	//Run all created ECs
	for (size_t worker = 0; worker < PARALLEL_ECS - 1; worker++){
		mythos::ExecutionContext ec(mythos::init::APP_CAP_START+3+worker*3);
		res1 = ec.run(res1.reuse());
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);
	}

	//participate in calling the object
	thread_mobileKernelObjectLatency(res1.reuse(), msg_ptr, mythos::init::APP_CAP_START);
}

void localKernelObjectLatency(){

	//Create a "homed" example object
	mythos::Example example(mythos::init::APP_CAP_START);
	auto res1 = example.create(myPortal, kmem, mythos::init::EXAMPLE_HOME_FACTORY);
	res1.wait();
	ASSERT(res1.state() == mythos::Error::SUCCESS);

	uint64_t start, end;

	//Measure the invocation latency to all available HWTs
	for(size_t place = 0; place < AVAILABLE_HWTS; place++){

		res1 = example.moveHome(res1.reuse(), place);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);

		for (size_t i = 0; i < NUM_RUNS; i++){
			start = getTime();
			res1 = example.ping(res1.reuse(),0);
			res1.wait();
			ASSERT(res1.state() == mythos::Error::SUCCESS);
			end = getTime();
			if (end < start){
				i--;
				continue;
			}
			MLOG_ERROR(mlog::app, "Invocation Latency to HWT ", place, " : ", end-start);
		}
	}
}

void executionContextCreationLatencyBundled(){

	mythos::PortalFutureRef<void> res1 = mythos::PortalFutureRef<void>(myPortal);
	uint64_t start, end;
	start = getTime();
	//Create new ECs for all HWTs besides the current one
	for (size_t worker = 0; worker < AVAILABLE_HWTS - 1; worker++){
		//Create a new invocation buffer
		mythos::Frame new_msg_ptr(mythos::init::APP_CAP_START+1+worker*3);
		res1 = new_msg_ptr.create(res1.reuse(), kmem, mythos::init::MEMORY_REGION_FACTORY, 1<<21, 1<<21);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);

		//Map the invocation buffer frame into user space memory
		mythos::InvocationBuf* ib = (mythos::InvocationBuf*)(((11+worker)<<21));

		auto res3 = myAS.mmap(res1.reuse(), new_msg_ptr, (uintptr_t)ib, 1<<21, mythos::protocol::PageMap::MapFlags().writable(true));
		res3.wait();
		ASSERT(res3.state() == mythos::Error::SUCCESS);

		//Create a second portal
		mythos::Portal portal2(mythos::init::APP_CAP_START+2+worker*3, ib);
		res1 = portal2.create(res3.reuse(), kmem, mythos::init::PORTAL_FACTORY);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);

		//Create a second EC on its own HWT
		mythos::ExecutionContext ec(mythos::init::APP_CAP_START+3+worker*3);
		res1 = ec.create(res1.reuse(), kmem, mythos::init::EXECUTION_CONTEXT_FACTORY,
				myAS, myCS, mythos::init::SCHEDULERS_START+1+worker,
				initstack_top+stacksize-4096*worker, &thread_main, (void*)worker);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);

		//Bind the new portal to the new EC
		res1 = portal2.bind(res1.reuse(), new_msg_ptr, 0, mythos::init::APP_CAP_START+3+worker*3);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);
	}

	//Run all created ECs
	for (size_t worker = 0; worker < AVAILABLE_HWTS - 1; worker++){
		mythos::ExecutionContext ec(mythos::init::APP_CAP_START+3+worker*3);
		res1 = ec.run(res1.reuse());
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);
	}
	MLOG_ERROR(mlog::app, "Create+Run of ", AVAILABLE_HWTS-1, "HWTs: ", end - start);
}

void executionContextCreationLatencySeparate(){
	mythos::PortalFutureRef<void> res1 = mythos::PortalFutureRef<void>(myPortal);

	uint64_t start, mid, end;

	//Create new ECs for all HWTs besides the current one
	for (size_t worker = 0; worker < AVAILABLE_HWTS - 1; worker++){
		start = getTime();
		//Create a new invocation buffer
		mythos::Frame new_msg_ptr(mythos::init::APP_CAP_START+1+worker*3);
		res1 = new_msg_ptr.create(res1.reuse(), kmem, mythos::init::MEMORY_REGION_FACTORY, 1<<21, 1<<21);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);

		//Map the invocation buffer frame into user space memory
		mythos::InvocationBuf* ib = (mythos::InvocationBuf*)(((11+worker)<<21));

		auto res3 = myAS.mmap(res1.reuse(), new_msg_ptr, (uintptr_t)ib, 1<<21, mythos::protocol::PageMap::MapFlags().writable(true));
		res3.wait();
		ASSERT(res3.state() == mythos::Error::SUCCESS);

		//Create a second portal
		mythos::Portal portal2(mythos::init::APP_CAP_START+2+worker*3, ib);
		res1 = portal2.create(res3.reuse(), kmem, mythos::init::PORTAL_FACTORY);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);

		//Create a second EC on its own HWT
		mythos::ExecutionContext ec(mythos::init::APP_CAP_START+3+worker*3);
		res1 = ec.create(res1.reuse(), kmem, mythos::init::EXECUTION_CONTEXT_FACTORY,
				myAS, myCS, mythos::init::SCHEDULERS_START+1+worker,
				initstack_top+stacksize-4096*worker, &thread_main, (void*)worker);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);

		//Bind the new portal to the new EC
		res1 = portal2.bind(res1.reuse(), new_msg_ptr, 0, mythos::init::APP_CAP_START+3+worker*3);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);

		mid = getTime();

		res1 = ec.run(res1.reuse());
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);
		end = getTime();
		if (mid < start || end < mid){
			worker--;
			continue;
		}
		MLOG_ERROR(mlog::app, "Create on HWT ", worker+1, ": ", mid - start, " Run: ", end - mid);
	}

}

void benchmarks(){
	//invocation latency to mobile kernel object
	//mobileKernelObjectLatency();

	//invocation latency to local kernel object
	//localKernelObjectLatency();

	//latency of creating and starting up execution contexts
	executionContextCreationLatencyBundled();
	//executionContextCreationLatencySeparate();
}

int main()
{
	char const begin[] = "Starting the benchmarks";
	char const end[] = "Benchmarks are done";
	mythos::syscall_debug(begin, sizeof(begin)-1);

	benchmarks();

	mythos::syscall_debug(end, sizeof(end)-1);

	return 0;
}
