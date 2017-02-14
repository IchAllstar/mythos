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

mythos::InvocationBuf* msg_ptr asm("msg_ptr");
int main() asm("main");

constexpr uint64_t stacksize = 240*4096;
char initstack[stacksize];
char* initstack_top = initstack+stacksize;

mythos::Portal portal(mythos::init::PORTAL, msg_ptr);
mythos::CapMap myCS(mythos::init::CSPACE);
mythos::PageMap myAS(mythos::init::PML4);
mythos::UntypedMemory kmem(mythos::init::UM);

char threadstack[stacksize];
char* thread1stack_top = threadstack+stacksize/2;
char* thread2stack_top = threadstack+stacksize;

void itoa(uint64_t value, char* buffer){
	for (size_t i = 0; i < 16; i++){
		memset(buffer+15-i, value % 10 + 48, 1); //ASCII code of that digit
		value /= 10;
	}
	memset(buffer+16, 0, 1);
}

void* thread_main(void* ctx)
{
	char dbg[] = "01234567890123456"; //Dummy String

	size_t num_runs = 10000;
//	mythos::Frame msg_ptr2(mythos::init::APP_CAP_START);
////	mythos::Portal portal2(mythos::init::APP_CAP_START+1, &msg_ptr2);
//
//	auto res2 = msg_ptr2.info(portal2);
//	res2.wait();
//	itoa(uint64_t(res2.state()), dbg);
//	mythos::syscall_debug(dbg, sizeof(dbg));
//	ASSERT(res2.state() == mythos::Error::SUCCESS);
//	if (msg_ptr->read<mythos::protocol::Frame::Info>().size)
//		itoa(msg_ptr->read<mythos::protocol::Frame::Info>().addr, dbg);
//	else
//		itoa(2, dbg);
//	mythos::syscall_debug(dbg, sizeof(dbg));


	size_t ownid = (size_t)ctx;
//	itoa(ownid, dbg);
//	mythos::syscall_debug(dbg, sizeof(dbg));

	mythos::InvocationBuf* ib2 = (mythos::InvocationBuf*)(((11+ownid)<<21));//(mythos::InvocationBuf*)(msg_ptr->read<mythos::protocol::Frame::Info>().addr);
	mythos::Portal portal2(mythos::init::APP_CAP_START+2+ownid*3, ib2);

	mythos::Example example(mythos::init::APP_CAP_START);

	auto res1 = example.ping(portal2);
	res1.wait();
	ASSERT(res1.state() == mythos::Error::SUCCESS);

	//Invocation latency to mobile kernel obejct
	{
		uint64_t start, end;
		char str[] = "01234567890123456"; //Dummy String


		for (size_t i = 0; i < num_runs; i++){
			asm volatile("rdtsc":"=A"(start));
			res1 = example.ping(res1.reuse());
			res1.wait();
			ASSERT(res1.state() == mythos::Error::SUCCESS);
			asm volatile("rdtsc":"=A"(end));
			if (end < start){
				i--;
				continue;
			}
			itoa(end - start, str);
			mythos::syscall_debug(str, sizeof(str));
		}

	}


  char const str[] = "hello thread!";
  mythos::syscall_debug(str, sizeof(str)-1);
  return 0;
}

void benchmarks(){
	char dbg[] = "01234567890123456"; //Dummy String

	//Number of iterations per benchmark
	size_t num_runs = 10000;
	size_t parallel_ecs = 10;

	//Create a mobile kernel object
	mythos::Example example(mythos::init::APP_CAP_START);
	auto res1 = example.create(portal, kmem, mythos::init::EXAMPLE_FACTORY);
	res1.wait();
	ASSERT(res1.state() == mythos::Error::SUCCESS);

	res1 = example.ping(res1.reuse());
	res1.wait();
	ASSERT(res1.state() == mythos::Error::SUCCESS);


	//Create (parallel_ecs - 1) additional ECs
	for (size_t worker = 0; worker < parallel_ecs - 1; worker++){
		//Create a second invocation buffer
		mythos::Frame msg_ptr2(mythos::init::APP_CAP_START+1+worker*3);
		res1 = msg_ptr2.create(res1.reuse(), kmem, mythos::init::MEMORY_REGION_FACTORY, 1<<21, 1<<21);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);

		mythos::InvocationBuf* ib2 = (mythos::InvocationBuf*)(((11+worker)<<21));

		auto res3 = myAS.mmap(res1.reuse(), msg_ptr2, (uintptr_t)ib2, 1<<21, mythos::protocol::PageMap::MapFlags().writable(true));
		res3.wait();
		ASSERT(res3.state() == mythos::Error::SUCCESS);

		//Create a second portal
		mythos::Portal portal2(mythos::init::APP_CAP_START+2+worker*3, ib2);
		res1 = portal2.create(res3.reuse(), kmem, mythos::init::PORTAL_FACTORY);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);

		//Create a second EC on HWT x
		mythos::ExecutionContext ec2(mythos::init::APP_CAP_START+3+worker*3);
		res1 = ec2.create(res1.reuse(), kmem, mythos::init::EXECUTION_CONTEXT_FACTORY,
				myAS, myCS, mythos::init::SCHEDULERS_START+1+worker,
				thread2stack_top+4096*worker, &thread_main, (void*)worker);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);

		//Bind the new portal to the new EC
		res1 = portal2.bind(res1.reuse(), msg_ptr2, 0, mythos::init::APP_CAP_START+3+worker*3);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);

		//Start the new EC
		res1 = ec2.run(res1.reuse());
		mythos::syscall_debug(dbg, sizeof(dbg));
		ASSERT(res1.state() == mythos::Error::SUCCESS);
	}


	//Invocation latency to mobile kernel obejct
	{
		uint64_t start, end;
		char str[] = "01234567890123456"; //Dummy String


		for (size_t i = 0; i < num_runs; i++){
			asm volatile("rdtsc":"=A"(start));
			res1 = example.ping(res1.reuse());
			res1.wait();
			ASSERT(res1.state() == mythos::Error::SUCCESS);
			asm volatile("rdtsc":"=A"(end));
			if (end < start){
				i--;
				continue;
			}
			itoa(end - start, str);
			mythos::syscall_debug(str, sizeof(str));
		}

	}
	while(true);




	//invocation latency to local kernel object
	{
		mythos::Example example(mythos::init::APP_CAP_START+3);
		res1 = example.create(res1.reuse(), kmem, mythos::init::EXAMPLE_HOME_FACTORY);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);

		uint64_t start, end;
		char str[] = "01234567890123456"; //Dummy String

		for(size_t place = 0; place < 240; place++){

			res1 = example.moveHome(res1.reuse(), place);
			res1.wait();
			ASSERT(res1.state() == mythos::Error::SUCCESS);

			for (size_t i = 0; i < num_runs; i++){
				asm volatile("rdtsc":"=A"(start));
				res1 = example.ping(res1.reuse());
				res1.wait();
				ASSERT(res1.state() == mythos::Error::SUCCESS);
				asm volatile("rdtsc":"=A"(end));
				if (end < start){
					i--;
					continue;
				}
				itoa(end - start, str);
				mythos::syscall_debug(str, sizeof(str));
			}
		}


	}

//	mythos::ExecutionContext ec1(mythos::init::APP_CAP_START);
//	auto res1 = ec1.create(portal, kmem, mythos::init::EXECUTION_CONTEXT_FACTORY,
//			myAS, myCS, mythos::init::SCHEDULERS_START,
//			thread1stack_top, &thread_main, nullptr);
//	res1.wait();
//	ASSERT(res1.state() == mythos::Error::SUCCESS);
//
//	mythos::ExecutionContext ec2(mythos::init::APP_CAP_START+1);
//	auto res2 = ec2.create(res1.reuse(), kmem, mythos::init::EXECUTION_CONTEXT_FACTORY,
//			myAS, myCS, mythos::init::SCHEDULERS_START+1,
//			thread2stack_top, &thread_main, nullptr);
//	res2.wait();
//	ASSERT(res2.state() == mythos::Error::SUCCESS);
}

int main()
{
  char const begin[] = "Starting the benchmarks";
  char const obj[] = "hello object!";
  char const end[] = "Benchmarks are done";
  mythos::syscall_debug(begin, sizeof(begin)-1);

  benchmarks();

  mythos::syscall_debug(end, sizeof(end)-1);

  return 0;
}
