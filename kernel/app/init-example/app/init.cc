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
#include "runtime/PageMap.hh"
#include "runtime/KernelMemory.hh"
#include "runtime/SimpleCapAlloc.hh"
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
mythos::KernelMemory kmem(mythos::init::KM);
mythos::SimpleCapAllocDel capAlloc(portal, myCS, mythos::init::APP_CAP_START,
                                  mythos::init::SIZE-mythos::init::APP_CAP_START);

<<<<<<< HEAD
char threadstack[stacksize];
char* thread1stack_top = threadstack+stacksize/2;
char* thread2stack_top = threadstack+stacksize;
=======
// Benchmark Parameter
constexpr uint64_t REPETITIONS = 10;

// PING PONG BENCHMARK PARAMETER
uint64_t thread1   = 1;
uint64_t thread2   = 3;
Policy   policy    = {Policy::SLEEP};
uint64_t pingPongs = 100;


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
uint64_t getTime() {
  uint64_t hi, lo;
  asm volatile("rdtsc":"=a"(lo), "=d"(hi));
  return ((uint64_t)lo) | ( ((uint64_t)hi) << 32);
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

  void create_thread_on_sc(uint64_t dest_offset, uint64_t sc, void* data, mythos::Portal &portal) {

    mythos::PortalLock pl(portal); // future access will fail if the portal is in use already

    //Create a new invocation buffer
    //MLOG_ERROR(mlog::app, "Create IB", DVAR(dest_offset));
    mythos::Frame new_msg_ptr(FRAME_CAP_START + dest_offset);
    auto res = new_msg_ptr.create(pl, kmem, 1 << 21, 1 << 21).wait();
    ASSERT(res);

    //Map the invocation buffer frame into user space memory
    //MLOG_ERROR(mlog::app, "Map IB");
    mythos::InvocationBuf* ib = (mythos::InvocationBuf*)(((11 + dest_offset) << 21));

    auto res3 = myAS.mmap(pl,
                          new_msg_ptr, (uintptr_t)ib, 1 << 21,
                          mythos::protocol::PageMap::MapFlags().writable(true)).wait();
    ASSERT(res3);


    // Create a new portal for thread
    //MLOG_ERROR(mlog::app, "Create Portal");
    mythos::Portal portal_(PORTAL_CAP_START + dest_offset, ib);
    res = portal_.create(pl, kmem).wait();
    ASSERT(res);


    //Create Execution Context
    //MLOG_ERROR(mlog::app, "Create EC", dest_offset);
    mythos::ExecutionContext ec(mythos::init::APP_CAP_START + dest_offset);
    res = ec.create(pl, kmem, myAS, myCS, mythos::init::SCHEDULERS_START + sc,
                    threadstack_top - ONE_STACK * dest_offset, thread_func, data).wait();
    ASSERT(res);

    //Bind the new portal to the new EC
    res = portal_.bind(pl, new_msg_ptr, 0, mythos::init::APP_CAP_START + dest_offset).wait();
    ASSERT(res);
  }

  void create_thread(uint64_t dest_offset, void* data, mythos::Portal &portal) {
    create_thread_on_sc(dest_offset, dest_offset, data, portal);
  }

  void delete_thread(uint64_t dest_offset, mythos::Portal &portal) {
    mythos::PortalLock pl(portal);
    mythos::CapMap capmap(mythos::init::CSPACE);

    //MLOG_ERROR(mlog::app, "delet frame");
    auto res = capmap.deleteCap(pl, FRAME_CAP_START + dest_offset).wait();
    ASSERT(res);
    //MLOG_ERROR(mlog::app, "delet ec");
    res = capmap.deleteCap(pl, mythos::init::APP_CAP_START + dest_offset).wait();
    ASSERT(res);
    //MLOG_ERROR(mlog::app, "delet portal");
    res = capmap.deleteCap(pl, PORTAL_CAP_START + dest_offset).wait();
    ASSERT(res);
  }
private:
  void* (*thread_func)(void*);
};


/**
 * Benchmark n threads wait for wakeup
 */
void benchmark_wakeup_hwts() {
  uint64_t time = 0;
  uint64_t start, middle, end;
  auto fun = [](void *data) -> void* {
    while (true) {
      //MLOG_ERROR(mlog::app, "WAIT");
      mythos::ISysretHandler::handle(mythos::syscall_wait());
      //MLOG_ERROR(mlog::app, "RESUME");
      acknowledges.fetch_add(1);
    }
  };
  thread_manager test(fun);

  for (uint64_t i = 1; i < HARDWARE_THREADS; i++) {
    test.create_thread(i, (void*)i, portal);
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
      MLOG_ERROR(mlog::app, number, "start - end, middle - end", end - start, end - middle);
    }
  }
}

/**
 * Benchmark recursive threads
 */
/*
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
>>>>>>> 4db0abe... Some intents to find bug with scheduling EC's

void* thread_main(void* ctx)
{
  MLOG_INFO(mlog::app, "hello thread!", DVAR(ctx));
  mythos::ISysretHandler::handle(mythos::syscall_wait());
  MLOG_INFO(mlog::app, "thread resumed from wait", DVAR(ctx));
  return 0;
}

void test_Example()
{
  char const obj[] = "hello object!";
  MLOG_ERROR(mlog::app, "test_Example begin");
  mythos::PortalLock pl(portal); // future access will fail if the portal is in use already
  mythos::Example example(capAlloc());
  TEST(example.create(pl, kmem).wait()); // use default mythos::init::EXAMPLE_FACTORY
  // wait() waits until the result is ready and returns a copy of the data and state.
  // hence, the contents of res1 are valid even after the next use of the portal
  TEST(example.printMessage(pl, obj, sizeof(obj)-1).wait());
  TEST(capAlloc.free(example,pl));
  // pl.release(); // implicit by PortalLock's destructor
  MLOG_ERROR(mlog::app, "test_Example end");
}

<<<<<<< HEAD
void test_Portal()
{
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
  MLOG_INFO(mlog::app, "alloc frame", DVAR(res2.state()));
  TEST(res2);
  // map the frame into our address space
  MLOG_INFO(mlog::app, "test_Portal: map frame");
  auto res3 = myAS.mmap(pl, f, vaddr, 2*1024*1024, 0x1).wait();
  MLOG_INFO(mlog::app, "mmap frame", DVAR(res3.state()),
            DVARhex(res3->vaddr), DVARhex(res3->size), DVAR(res3->level));
  TEST(res3);
  // bind the portal in order to receive responses
  MLOG_INFO(mlog::app, "test_Portal: configure portal");
  auto res4 = p2.bind(pl, f, 0, mythos::init::EC).wait();
  TEST(res4);
  // and delete everything again
  MLOG_INFO(mlog::app, "test_Portal: delete frame");
  TEST(capAlloc.free(f, pl));
  MLOG_INFO(mlog::app, "test_Portal: delete portal");
  TEST(capAlloc.free(p2, pl));
  MLOG_ERROR(mlog::app, "test_Portal end");
=======
/**
 *
 *  Benchmark ping pong two threads
 *
 */
void* thread_ping_pong(void* ctx) {
  MLOG_ERROR(mlog::app, "started thread", ctx);
  mythos::InvocationBuf* ib = (mythos::InvocationBuf*)(((11 + (uint64_t)ctx) << 21));
  mythos::Portal portal(PORTAL_CAP_START + (uint64_t)ctx, ib);
  uint64_t start, end;

  uint64_t id = (uint64_t)ctx;
  uint64_t other_id = (id == thread1) ? thread2 : thread1;

  uint64_t values[pingPongs];

  for (uint64_t i = 0; i < pingPongs; i++) {
    start = getTime();
    //MLOG_ERROR(mlog::app, "Wait",id);
    mythos::ISysretHandler::handle(mythos::syscall_wait());
    //MLOG_ERROR(mlog::app, "Notify",id);
    mythos::syscall_notify(mythos::init::APP_CAP_START + other_id);
    end = getTime();
    values[i] = end - start;
  }

  if (id != thread1) {
    return (void*)0;
  }
  for (int i = 0; i < pingPongs; i++) {
    MLOG_ERROR(mlog::app, i, values[i]);
  }
>>>>>>> 4db0abe... Some intents to find bug with scheduling EC's
}

void test_float()
{
  MLOG_INFO(mlog::app, "testing user-mode floating point");

  volatile float x = 5.5;
  volatile float y = 0.5;

  float z = x*y;

  TEST_EQ(int(z), 2);
  TEST_EQ(int(1000*(z-float(int(z)))), 750);
  MLOG_INFO(mlog::app, "float z:", int(z), ".", int(1000*(z-float(int(z)))));
}

struct HostChannel {
  void init() { ctrlToHost.init(); ctrlFromHost.init(); }
  typedef mythos::PCIeRingChannel<128,8> CtrlChannel;
  CtrlChannel ctrlToHost;
  CtrlChannel ctrlFromHost;
};

mythos::PCIeRingProducer<HostChannel::CtrlChannel> app2host;
mythos::PCIeRingConsumer<HostChannel::CtrlChannel> host2app;

int main()
{
  char const str[] = "hello world!";
  char const end[] = "bye, cruel world!";
  mythos::syscall_debug(str, sizeof(str)-1);
  MLOG_ERROR(mlog::app, "application is starting :)", DVARhex(msg_ptr), DVARhex(initstack_top));

  test_float();
  test_Example();
  test_Portal();

  {
    mythos::PortalLock pl(portal); // future access will fail if the portal is in use already
    // allocate a 2MiB frame
    mythos::Frame hostChannelFrame(capAlloc());
    auto res1 = hostChannelFrame.create(pl, kmem, 2*1024*1024, 2*1024*1024).wait();
    MLOG_INFO(mlog::app, "alloc hostChannel frame", DVAR(res1.state()));
    TEST(res1);

    // map the frame into our address space
    uintptr_t vaddr = 22*1024*1024;
    auto res2 = myAS.mmap(pl, hostChannelFrame, vaddr, 2*1024*1024, 0x1).wait();
    MLOG_INFO(mlog::app, "mmap hostChannel frame", DVAR(res2.state()),
              DVARhex(res2.get().vaddr), DVARhex(res2.get().size), DVAR(res2.get().level));
    TEST(res2);

    // initialise the memory
    HostChannel* hostChannel = reinterpret_cast<HostChannel*>(vaddr);
    hostChannel->init();
    host2app.setChannel(&hostChannel->ctrlFromHost);
    app2host.setChannel(&hostChannel->ctrlToHost);

    // register the frame in the host info table
    // auto res3 = mythos::PortalFuture<void>(pl.invoke<mythos::protocol::CpuDriverKNC::SetInitMem>(mythos::init::CPUDRIVER, hostChannelFrame.cap())).wait();
    // MLOG_INFO(mlog::app, "register at host info table", DVAR(res3.state()));
    //ASSERT(res3.state() == mythos::Error::SUCCESS);
  }

  mythos::ExecutionContext ec1(capAlloc());
  mythos::ExecutionContext ec2(capAlloc());
  {
    MLOG_INFO(mlog::app, "test_EC: create ec1");
    mythos::PortalLock pl(portal); // future access will fail if the portal is in use already
    auto res1 = ec1.create(pl, kmem, myAS, myCS, mythos::init::SCHEDULERS_START,
                           thread1stack_top, &thread_main, nullptr).wait();
    TEST(res1);
    MLOG_INFO(mlog::app, "test_EC: create ec2");
    auto res2 = ec2.create(pl, kmem, myAS, myCS, mythos::init::SCHEDULERS_START+1,
                           thread2stack_top, &thread_main, nullptr).wait();
    TEST(res2);
  }

  for (volatile int i=0; i<100000; i++) {
    for (volatile int j=0; j<1000; j++) {}
  }

<<<<<<< HEAD
  MLOG_INFO(mlog::app, "sending notifications");
  mythos::syscall_notify(ec1.cap());
  mythos::syscall_notify(ec2.cap());
=======
void benchmarks() {
  benchmark_wakeup_hwts();
  //benchmark_wakeup_recursive();
  //benchmark_kernel_object_access();
  //benchmark_ping_pong();
  //benchmark_syscall_latencies();
}
>>>>>>> 4db0abe... Some intents to find bug with scheduling EC's

  mythos::syscall_debug(end, sizeof(end)-1);

  return 0;
}
