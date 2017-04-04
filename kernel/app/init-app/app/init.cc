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

// needed or program crashes with various errors because no stack for the main()
char initstack[ONE_STACK];
char* initstack_top = initstack+ONE_STACK;


constexpr uint64_t STACKSIZE = AVAILABLE_HWTS*ONE_STACK;
constexpr uint64_t PORTAL_CAP_START = 1500;
constexpr uint64_t FRAME_CAP_START = 1750;



mythos::Portal myPortal(mythos::init::PORTAL, msg_ptr);
mythos::CapMap myCS(mythos::init::CSPACE);
mythos::PageMap myAS(mythos::init::PML4);
mythos::UntypedMemory kmem(mythos::init::UM);

// following line crashes Creation of threads? init stack? other memory issue?
//mythos::PortalFutureRef<void> global_pfr = mythos::PortalFutureRef<void>(myPortal);

char threadstack[STACKSIZE];
char* threadstack_top = threadstack+STACKSIZE;


// Counting acknoledges of threads
std::atomic<uint64_t> acknowledges = {0};

uint64_t getTime(){
  uint64_t hi, lo;
  asm volatile("rdtsc":"=a"(lo), "=d"(hi));
  return ((uint64_t)lo)|( ((uint64_t)hi)<<32);
}

class thread_manager {
  public:
    thread_manager( void*(*func)(void*) ) {
      thread_func = func;
    }

    void create_thread(uint64_t dest_offset, void* data,  mythos::PortalFutureRef<void> &res) {
      //Create a new invocation buffer

      //MLOG_ERROR(mlog::app, "Create IB");
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

    void run_thread(uint64_t dest_offset, mythos::PortalFutureRef<void> res) {
    //MLOG_ERROR(mlog::app, "run");
    mythos::ExecutionContext ec(mythos::init::APP_CAP_START+dest_offset);
    res = ec.run(res.reuse());
    res.wait();
    ASSERT(res.state() == mythos::Error::SUCCESS);
    //MLOG_ERROR(mlog::app, "run fin");
    }

    void delete_thread(uint64_t dest_offset, mythos::PortalFutureRef<void> res) {
/*
      mythos::InvocationBuf* ib = (mythos::InvocationBuf*)(((11+dest_offset)<<21));
      auto res3 = myAS.munmap(res.reuse(), (uintptr_t)ib, 1<<21);
      res3.wait();
      ASSERT(res3.state() == mythos::Error::SUCCESS);
*/
      mythos::CapMap capmap(mythos::init::CSPACE);


   //   MLOG_ERROR(mlog::app, "delet frame");
      res = capmap.deleteCap(res.reuse(), FRAME_CAP_START + dest_offset);
      res.wait();
      ASSERT(res.state() == mythos::Error::SUCCESS);
 //     MLOG_ERROR(mlog::app, "delet portal");
      res = capmap.deleteCap(res.reuse(), PORTAL_CAP_START + dest_offset);
      res.wait();
      ASSERT(res.state() == mythos::Error::SUCCESS);

      //MLOG_ERROR(mlog::app, "delet ec");
      res = capmap.deleteCap(res.reuse(), mythos::init::APP_CAP_START + dest_offset);
      res.wait();
      ASSERT(res.state() == mythos::Error::SUCCESS);
    }
  private:
    void* (*thread_func)(void*);
};

void* thread_main(void* ctx){
 MLOG_ERROR(mlog::app, "Hello from Thread ", (uint64_t)ctx);
  //acknowledges.fetch_add(1);
  return 0;
}


/**
 * Benchmark recursive threads
 */
void benchmark_kernel_object_access() {

}


/**
 * Benchmark n threads wait for wakeup
 */
void benchmark_wakeup_hwts() {
  uint64_t time = 0;
  uint64_t start, end;
  mythos::PortalFutureRef<void> res1 = mythos::PortalFutureRef<void>(myPortal);
  auto fun = [](void *data) -> void* { acknowledges.fetch_add(1); };
  thread_manager test(fun);

  for (uint64_t number = 1; number < AVAILABLE_HWTS - 1; number++) {
    MLOG_ERROR(mlog::app, "create");
    for (uint64_t i = 0; i < number; i++) {
      test.create_thread(i + 1, (void*)i, res1);
    }
    MLOG_ERROR(mlog::app, "run");
    start = getTime();
    for (uint64_t i = 0; i < number; i++) {
      test.run_thread(i + 1, res1);
    }

    while (acknowledges != number) {};
    end = getTime();
    acknowledges = 0;
    MLOG_ERROR(mlog::app, number,  " ", end - start);

    MLOG_ERROR(mlog::app, "delete");
    for (uint64_t i = 0; i < number; i++) {
      test.delete_thread(i + 1, res1);
    }
  }
  /*for (uint64_t i = 1; i < AVAILABLE_HWTS; i++) {
    //MLOG_ERROR(mlog::app, "init");
    simple_init_ecs(i, res1);
    //MLOG_ERROR(mlog::app, "run");
    time = simple_run_ecs(i, res1);
    //MLOG_ERROR(mlog::app, "delete");
    delete_threads(i, res1);
    MLOG_ERROR(mlog::app,"HWT:", i, " took cycles: ", time);
  }
  */

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
constexpr uint64_t  NO_RECURSE = 200;

// defines how big a partition is
constexpr uint64_t SPLIT_SIZE = 200;
void* thread_func_recursive(void* ctx) {
  thread_msg msg(ctx);

  //MLOG_ERROR(mlog::app, "START " , msg.start_id, " END ", msg.dest_id, " HEX ", DVARhex(ctx));
  thread_manager mng(&thread_func_recursive);
  mythos::InvocationBuf* ib = (mythos::InvocationBuf*)(((11+(uint64_t)msg.start_id)<<21));
  mythos::Portal portal(PORTAL_CAP_START + msg.start_id, ib);
  mythos::PortalFutureRef<void> res1 = mythos::PortalFutureRef<void>(portal);

  uint64_t thread_count = msg.dest_id - msg.start_id;
  if (thread_count > NO_RECURSE) { //split and recurse
    //MLOG_ERROR(mlog::app, "RECURSE", DVAR(msg.start_id), DVAR(msg.dest_id));
    uint64_t counter = msg.start_id + 1;
    while (counter <= msg.dest_id) {
      uint64_t dest = (counter + SPLIT_SIZE > msg.dest_id) ? msg.dest_id : counter + SPLIT_SIZE;
      //MLOG_ERROR(mlog::app, "Create thread", counter, " responsible for " , counter, " to " , dest);
      mng.create_thread(counter, thread_msg(counter, dest).to_void(), res1);
      //MLOG_ERROR(mlog::app, "Done");
      mng.run_thread(counter, res1);
      counter += SPLIT_SIZE + 1;
    }

  } else { // directly create the thread_count threads
    //MLOG_ERROR(mlog::app, "NO RECURSE",DVAR(msg.start_id), DVAR(thread_count));
    for (uint64_t i = 1; i < thread_count + 1; i++) {
      mng.create_thread(msg.start_id + i, thread_msg(msg.start_id+i, msg.start_id+i).to_void(), res1);
      mng.run_thread(msg.start_id + i, res1);
    }
  }

  //actual task after creation
  acknowledges.fetch_add(1);
}

void benchmark_wakeup_recursive() {
  uint64_t start, end;
  thread_manager mng(&thread_func_recursive);
  mythos::PortalFutureRef<void> res1 = mythos::PortalFutureRef<void>(myPortal);
  uint64_t number = 199;
  thread_msg msg(1, number);
  mng.create_thread(1, msg.to_void(), res1);
  start = getTime();
  mng.run_thread(1, res1);
  while(acknowledges != number) {};
  end = getTime();
  MLOG_ERROR(mlog::app, "HWT:", number, end - start);
}

void benchmarks(){
  benchmark_wakeup_hwts();
  //benchmark_wakeup_recursive();
  //benchmark_kernel_object_access();

/*
  mythos::PortalFutureRef<void> res1 = mythos::PortalFutureRef<void>(myPortal);
  thread_manager test(&func);
  test.create_thread(1, (void*)0, res1);
  test.run_thread(1, res1);
*/
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
