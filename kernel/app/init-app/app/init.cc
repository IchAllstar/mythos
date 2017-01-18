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

constexpr uint64_t stacksize = 4*4096;
char initstack[stacksize];
char* initstack_top = initstack+stacksize;

mythos::Portal portal(mythos::init::PORTAL, msg_ptr);
mythos::CapMap myCS(mythos::init::CSPACE);
mythos::PageMap myAS(mythos::init::PML4);
mythos::UntypedMemory kmem(mythos::init::UM);

char threadstack[stacksize];
char* thread1stack_top = threadstack+stacksize/2;
char* thread2stack_top = threadstack+stacksize;

void* thread_main(void* ctx)
{
  char const str[] = "hello thread!";
  mythos::syscall_debug(str, sizeof(str)-1);
  return 0;
}

void itoa(uint64_t value, char* buffer){
	for (size_t i = 0; i < 16; i++){
		memset(buffer+15-i, value % 10 + 48, 1); //ASCII code of that digit
		value /= 10;
	}
	memset(buffer+16, 0, 1);
}

void benchmarks(){

	//Number of iterations per benchmark
	size_t num_runs = 100;

	//invocation latency to local kernel object
	{
		mythos::Example example(mythos::init::APP_CAP_START);
		auto res1 = example.create(portal, kmem, mythos::init::EXAMPLE_HOME_FACTORY);
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);

		uint64_t start, end;
		char str[] = "01234567890123456"; //Dummy String

		for (size_t i = 0; i < num_runs; i++){
			asm volatile("rdtsc":"=A"(start));
			res1 = example.ping(res1.reuse());
			res1.wait();
			ASSERT(res1.state() == mythos::Error::SUCCESS);
			asm volatile("rdtsc":"=A"(end));
			itoa(end - start, str);
			mythos::syscall_debug(str, sizeof(str));
		}

		res1 = example.moveHome(res1.reuse(), 5);
		asm volatile("rdtsc":"=A"(start));
		res1 = example.ping(res1.reuse());
		res1.wait();
		ASSERT(res1.state() == mythos::Error::SUCCESS);
		asm volatile("rdtsc":"=A"(end));
		itoa(end - start, str);
		mythos::syscall_debug(str, sizeof(str));

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
