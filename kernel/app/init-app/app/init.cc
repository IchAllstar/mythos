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

using mythos::CapPtr;

mythos::InvocationBuf* msg_ptr asm("msg_ptr");
int main() asm("main");

constexpr uint64_t stacksize = 4*4096;
char initstack[stacksize];
char* initstack_top = initstack+stacksize;

mythos::Portal portal(mythos::init::PORTAL, msg_ptr);
mythos::CapMap myCS(mythos::init::CSPACE);
mythos::PageMap myAS(mythos::init::PML4);
mythos::UntypedMemory kmem(mythos::init::UM);

// Benchmark Parameter
constexpr uint64_t REPETITIONS = 10;

// Capability Ranges
constexpr uint64_t PORTAL_CAP_START = 1500;
constexpr uint64_t FRAME_CAP_START = 1750;

/// Stack for created threads
constexpr uint64_t ONE_STACK = 4096 * 2;
constexpr uint64_t MAX_HARDWARE_THREADS = 200;
constexpr uint64_t HARDWARE_THREADS = 200;
constexpr uint64_t THREAD_STACKSIZE = ONE_STACK * HARDWARE_THREADS;
char threadstack[THREAD_STACKSIZE];
char *threadstack_top = threadstack + THREAD_STACKSIZE;

// Counting acknoledges of threads
std::atomic<uint64_t> acknowledges = {0};

// not synchronized accross SC's
uint64_t getTime(){
  uint64_t hi, lo;
  asm volatile("rdtsc":"=a"(lo), "=d"(hi));
  return ((uint64_t)lo)|( ((uint64_t)hi)<<32);
}

struct CapAlloc {
  CapAlloc(mythos::CapPtr cap) : cap(cap) {}
  mythos::CapPtr alloc() { return cap++; }
  mythos::CapPtr cap;
} caps(mythos::init::APP_CAP_START);

typedef struct thread {
	CapPtr ec_cap;
	CapPtr sc_cap;
	CapPtr portal_cap;
	CapPtr frame_cap;
	void *tos;
} thread;
thread thread_array[MAX_HARDWARE_THREADS];

/**
 *
 * Thread Manager
 *
 */
class thread_manager {
  public:
    thread_manager( void*(*func)(void*) ) {
      thread_func = func;
    }

    void create_thread(uint64_t dest_offset, void* data,  mythos::PortalFutureRef<void> &res) {
      //Create a new invocation buffer

      //MLOG_ERROR(mlog::app, "Create IB", DVAR(dest_offset));
      mythos::Frame new_msg_ptr(FRAME_CAP_START + dest_offset);
      res = new_msg_ptr.create(res.reuse(), kmem,
          mythos::init::MEMORY_REGION_FACTORY, 1<<21, 1<<21);
      res.wait();
      ASSERT(res.state() == mythos::Error::SUCCESS);

      //Map the invocation buffer frame into user space memory
      //MLOG_ERROR(mlog::app, "Map IB");
      mythos::InvocationBuf* ib = (mythos::InvocationBuf*)(((11+dest_offset)<<21));

      auto res3 = myAS.mmap(res.reuse(),
          new_msg_ptr, (uintptr_t)ib, 1<<21,
          mythos::protocol::PageMap::MapFlags().writable(true));
      res3.wait();
      ASSERT(res3.state() == mythos::Error::SUCCESS);


      // Create a new portal for thread
      //MLOG_ERROR(mlog::app, "Create Portal");
      mythos::Portal portal(PORTAL_CAP_START + dest_offset, ib);
      res = portal.create(res3.reuse(), kmem,
          mythos::init::PORTAL_FACTORY);
      res.wait();
      ASSERT(res.state() == mythos::Error::SUCCESS);


      //Create Execution Context
      //MLOG_ERROR(mlog::app, "Create EC", dest_offset);
      mythos::ExecutionContext ec(mythos::init::APP_CAP_START+dest_offset);
      res = ec.create(res.reuse(), kmem, mythos::init::EXECUTION_CONTEXT_FACTORY,
          myAS, myCS, mythos::init::SCHEDULERS_START+dest_offset,
          threadstack_top-ONE_STACK*dest_offset, thread_func, data);
      res.wait();
      ASSERT(res.state() == mythos::Error::SUCCESS);

      //Bind the new portal to the new EC
      res = portal.bind(res.reuse(), new_msg_ptr, 0, mythos::init::APP_CAP_START+dest_offset);
      res.wait();
      ASSERT(res.state() == mythos::Error::SUCCESS);
    }

    void delete_thread(uint64_t dest_offset, mythos::PortalFutureRef<void> res) {
      mythos::CapMap capmap(mythos::init::CSPACE);


   	  //MLOG_ERROR(mlog::app, "delet frame");
      res = capmap.deleteCap(res.reuse(), FRAME_CAP_START + dest_offset);
      res.wait();
      ASSERT(res.state() == mythos::Error::SUCCESS);
      //MLOG_ERROR(mlog::app, "delet ec");
      res = capmap.deleteCap(res.reuse(), mythos::init::APP_CAP_START + dest_offset);
      res.wait();
      ASSERT(res.state() == mythos::Error::SUCCESS);
 	  //MLOG_ERROR(mlog::app, "delet portal");
      res = capmap.deleteCap(res.reuse(), PORTAL_CAP_START + dest_offset);
      res.wait();
      ASSERT(res.state() == mythos::Error::SUCCESS);
    }
  private:
    void* (*thread_func)(void*);
};


/**
 * Benchmark n threads wait for wakeup
 */
void benchmark_wakeup_hwts() {
  uint64_t time = 0;
  uint64_t start,middle, end;
  mythos::PortalFutureRef<void> res1 = mythos::PortalFutureRef<void>(portal);
  auto fun = [](void *data) -> void* {
    while(true) {
    //MLOG_ERROR(mlog::app, "WAIT");
    mythos::ISysretHandler::handle(mythos::syscall_wait());
    //MLOG_ERROR(mlog::app, "RESUME");
    acknowledges.fetch_add(1);
    }
  };
  thread_manager test(fun);

  for (uint64_t i = 1; i < HARDWARE_THREADS; i++) {
    test.create_thread(i, (void*)i, res1);
  }
  for (uint64_t number = 1; number < HARDWARE_THREADS; number++) {
    for (uint64_t repetitions = 0; repetitions < REPETITIONS; repetitions++) {
      start = getTime();
      for (uint64_t i = 1; i <= number; i++) {
        mythos::syscall_notify(mythos::init::APP_CAP_START + i);
      }
      middle = getTime();
      while (acknowledges != number) {};
      end = getTime();
      acknowledges = 0;
      MLOG_ERROR(mlog::app, number, "start middle end diff", start,middle, end, end - start, end - middle);
    }
  }
}

/**
 * Benchmark recursive threads
 */
/// message structure to give to thread
struct thread_msg {
  uint32_t start_id;
  uint32_t dest_id;

  thread_msg(uint32_t start_id_, uint32_t dest_id_)
    :start_id(start_id_), dest_id(dest_id_) {
  }

  thread_msg(void *data) {
    start_id = (uint32_t)(((uint64_t)data) >> 32);
    dest_id = (uint64_t)(data);
  }

  void* to_void() {
    uint64_t s = start_id;
    uint64_t d = dest_id;
    uint64_t ret = (s << 32) | dest_id;
    return (void*)ret;
  }
};

// defines if a thread splits the creation of its threads or creates them
// itself
constexpr uint64_t  NO_RECURSE = 10;

// defines how many  partition the range will be split into
constexpr uint64_t SPLIT_INTO = 2;
void* thread_func_recursive(void* ctx) {
  thread_msg msg(ctx);

  //MLOG_ERROR(mlog::app, "START " , msg.start_id, " END ", msg.dest_id, " HEX ", DVARhex(ctx));
  thread_manager mng(&thread_func_recursive);
  mythos::InvocationBuf* ib = (mythos::InvocationBuf*)(((11+(uint64_t)msg.start_id)<<21));
  mythos::Portal portal(PORTAL_CAP_START + msg.start_id, ib);
  mythos::PortalFutureRef<void> res1 = mythos::PortalFutureRef<void>(portal);

  uint64_t thread_count = msg.dest_id - msg.start_id;

  if (thread_count > NO_RECURSE) { //split and recurse
    MLOG_ERROR(mlog::app, "RECURSE", DVAR(msg.start_id), DVAR(msg.dest_id));
    uint64_t split_size = thread_count / SPLIT_INTO;
    uint64_t counter = msg.start_id + 1;
    while (counter <= msg.dest_id) {
      uint64_t dest = (counter + split_size > msg.dest_id) ? msg.dest_id : counter + split_size;
      MLOG_ERROR(mlog::app, "Create thread", counter, " responsible for " , counter, " to " , dest);
      mng.create_thread(counter, thread_msg(counter, dest).to_void(), res1);
      counter += split_size + 1;
    }

  } else { // directly create the thread_count threads
    //MLOG_ERROR(mlog::app, "NO RECURSE",DVAR(msg.start_id), DVAR(thread_count));
    for (uint64_t i = 1; i < thread_count + 1; i++) {
      //MLOG_ERROR(mlog::app, "Create", DVAR(msg.start_id + 1));
      mng.create_thread(msg.start_id + i, thread_msg(msg.start_id+i, msg.start_id+i).to_void(), res1);
    }
  }

  //actual task after creation
  acknowledges.fetch_add(1);
}

void benchmark_wakeup_recursive() {
  uint64_t start, end;
  thread_manager mng(&thread_func_recursive);
  mythos::PortalFutureRef<void> res1 = mythos::PortalFutureRef<void>(portal);

  for (uint64_t number = 90; number < HARDWARE_THREADS; number++) {
  	//MLOG_ERROR(mlog::app, "Round", number);
  	for (uint64_t repetitions = 0; repetitions < REPETITIONS; repetitions++) {
      thread_msg msg(1, number);
      start = getTime();
      mng.create_thread(1, msg.to_void(), res1);
      while(acknowledges != number) {};
      end = getTime();
      acknowledges = 0;
      MLOG_ERROR(mlog::app, "HWT:", number, end - start);
      for (uint64_t i = 0; i < number; i++) {
        mng.delete_thread(i+1, res1);
      }
  	}
  }
}

/**
 *
 *  Benchmark kernel object access
 *
 */
void* thread_object_access_func(void* ctx) {
  mythos::Example example(mythos::init::APP_CAP_START);
  mythos::InvocationBuf* ib = (mythos::InvocationBuf*)(((11+(uint64_t)ctx)<<21));
  mythos::Portal portal(PORTAL_CAP_START + (uint64_t)ctx, ib);
  mythos::PortalFutureRef<void> res1 = mythos::PortalFutureRef<void>(portal);
  uint64_t start, end;

  while (true) {
    mythos::ISysretHandler::handle(mythos::syscall_wait());
    start = getTime();
    for (uint64_t i = 0; i < 5000; i++) {
      res1 = example.printMessage(res1.reuse(), "HELLO", 5);
      res1.wait();
    }
    end = getTime();
    MLOG_ERROR(mlog::app, "Time for 5000 accesses", end - start);
    acknowledges.fetch_add(1);
  }
}

void benchmark_kernel_object_access() {
  char const obj[] = "hello object!";
  mythos::Example example(mythos::init::APP_CAP_START);
  auto res1 = example.create(portal, kmem, mythos::init::EXAMPLE_FACTORY);
  res1.wait();
  ASSERT(res1.state() == mythos::Error::SUCCESS);

  constexpr uint64_t MAX_ACCESSES = 20;
  thread_manager mng(&thread_object_access_func);
  // init all usable threads
  for (uint64_t i = 1; i < MAX_ACCESSES; i++) {
    mng.create_thread(i, (void*)i, res1);
    res1.wait();
    ASSERT(res1.state() == mythos::Error::SUCCESS);
  }
  for (uint64_t number = 2; number < MAX_ACCESSES; number++) {
    MLOG_ERROR(mlog::app,"Testing number", number);
    for (uint64_t i = 1; i <= number; i++) {
      mythos::syscall_notify(mythos::init::APP_CAP_START + i);
    }
    while (acknowledges != number) {};
    acknowledges = 0;
  }
  res1 = myCS.deleteCap(res1.reuse(), example);
  res1.wait();
  ASSERT(res1.state() == mythos::Error::SUCCESS);
}

/**
 *
 *  Benchmark ping pong two threads
 *
 */
void* thread_ping_pong(void* ctx) {
  mythos::InvocationBuf* ib = (mythos::InvocationBuf*)(((11+(uint64_t)ctx)<<21));
  mythos::Portal portal(PORTAL_CAP_START + (uint64_t)ctx, ib);
  mythos::PortalFutureRef<void> res1 = mythos::PortalFutureRef<void>(portal);
  uint64_t start, end;

  uint64_t id = (uint64_t)ctx;
  uint64_t other_id = (id == 2) ? 100 : 2;

  mythos::ExecutionContext remote_ec(mythos::init::APP_CAP_START + other_id);

  uint64_t values[7];

  uint64_t sum = 0;
  uint64_t agregate = 0;
  uint64_t max_repetitions = 100;
  uint64_t index = 0;
  auto list = {1,10,100,1000,2000,5000,10000};
  for (auto number : list) {
    agregate = 0;
    for (auto repetitions = 0; repetitions < max_repetitions; repetitions++) {
      sum = 0;
      for (uint64_t i = 0; i < number; i++) {
        start = getTime();
        mythos::ISysretHandler::handle(mythos::syscall_wait());
        mythos::syscall_notify(mythos::init::APP_CAP_START + other_id);

        end = getTime();
        sum += end - start;
      }
      agregate += sum;
      //MLOG_ERROR(mlog::app,number, "time", sum);
    }
    values[index] = agregate / max_repetitions;
    MLOG_ERROR(mlog::app, number, agregate / max_repetitions);
    index++;
  }

  //output
  //index = 0;
  //for (auto number : list) {
  //  MLOG_ERROR(mlog::app, number, values[index]);
  //  index++;
  //}

}

void benchmark_ping_pong() {
  mythos::Example example(mythos::init::APP_CAP_START);
  auto res1 = example.create(portal, kmem, mythos::init::EXAMPLE_FACTORY);
  res1.wait();
  ASSERT(res1.state() == mythos::Error::SUCCESS);

  thread_manager mng(&thread_ping_pong);

  mng.create_thread(2, (void*)2, res1);
  res1.wait();
  ASSERT(res1.state() == mythos::Error::SUCCESS);


  mng.create_thread(100, (void*)100, res1);
  res1.wait();
  ASSERT(res1.state() == mythos::Error::SUCCESS);

  mythos::syscall_notify(mythos::init::APP_CAP_START + 2);

  res1 = myCS.deleteCap(res1.reuse(), example);
  res1.wait();
  ASSERT(res1.state() == mythos::Error::SUCCESS);

}

void benchmarks(){
  //benchmark_wakeup_hwts();
  //benchmark_wakeup_recursive();
  //benchmark_kernel_object_access();
  benchmark_ping_pong();
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
