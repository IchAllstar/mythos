#include "mythos/init.hh"
#include "mythos/invocation.hh"
#include "runtime/Portal.hh"
#include "runtime/ExecutionContext.hh"
#include "runtime/CapMap.hh"
#include "runtime/Example.hh"
#include "runtime/PageMap.hh"
#include "runtime/UntypedMemory.hh"
#include "runtime/SimpleCapAlloc.hh"
#include "runtime/SchedulingCoordinator.hh"
#include "app/mlog.hh"
#include <cstdint>
#include "util/optional.hh"

using mythos::CapPtr;
using mythos::Policy;

mythos::InvocationBuf* msg_ptr asm("msg_ptr");
int main() asm("main");

constexpr uint64_t stacksize = 4 * 4096;
char initstack[stacksize];
char* initstack_top = initstack + stacksize;

mythos::Portal portal(mythos::init::PORTAL, msg_ptr);
mythos::CapMap myCS(mythos::init::CSPACE);
mythos::PageMap myAS(mythos::init::PML4);
mythos::UntypedMemory kmem(mythos::init::UM);

// Benchmark Parameter
constexpr uint64_t REPETITIONS = 10;

// WAKEUP HWT BENCHMARKS
uint64_t hardware_threads = 4;
uint64_t wakeup_repetitions = 10;

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

  for (uint64_t i = 1; i < hardware_threads; i++) {
    test.create_thread(i, (void*)i, portal);
  }
  for (uint64_t number = 1; number < hardware_threads; number++) {
    for (uint64_t repetitions = 0; repetitions < wakeup_repetitions; repetitions++) {
      start = getTime();
      for (uint64_t i = 1; i <= number; i++) {
        mythos::syscall_notify(mythos::init::APP_CAP_START + i);
      }
      middle = getTime();
      while (acknowledges != number) {};
      end = getTime();
      acknowledges = 0;
      //MLOG_ERROR(mlog::app, number, "start - end, middle - end", end - start, end - middle);
      MLOG_ERROR(mlog::app, number, end - start);
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
  mythos::InvocationBuf* ib = (mythos::InvocationBuf*)(((11 + (uint64_t)ctx) << 21));
  mythos::Portal portal(PORTAL_CAP_START + (uint64_t)ctx, ib);
  mythos::PortalLock pl(portal);
  uint64_t start, end;

  uint64_t accesses = 100;
  uint32_t values[accesses];

  while (true) {
    mythos::ISysretHandler::handle(mythos::syscall_wait());

    for (uint64_t i = 0; i < accesses; i++) {
      start = getTime();
      auto res1 = example.printMessage(pl, "HELLO", 5).wait();
      end = getTime();
      values[i] = end - start;
    }

    if ((uint64_t) ctx == 2) {
      for (uint64_t i = 0; i < accesses; i++) {
        MLOG_ERROR(mlog::app, i, values[i]);
      }
    }
    acknowledges.fetch_add(1);
  }
}

void benchmark_kernel_object_access() {
  char const obj[] = "hello object!";
  mythos::PortalLock pl(portal);
  mythos::Example example(mythos::init::APP_CAP_START);
  auto res1 = example.create(pl, kmem, mythos::init::EXAMPLE_FACTORY).wait();
  ASSERT(res1);

  constexpr uint64_t MAX_ACCESSES = 50;
  constexpr uint64_t number_threads = 2;
  thread_manager mng(&thread_object_access_func);
  // init all usable threads
  for (uint64_t i = 0; i < MAX_ACCESSES; i++) {
    mng.create_thread(i, (void*)i, portal);
    ASSERT(res1);
  }

  //mythos::syscall_notify(mythos::init::APP_CAP_START + 1);
  //mythos::syscall_notify(mythos::init::APP_CAP_START + 2);

  for (int i = 1; i <= number_threads; i++) {
    mythos::syscall_notify(mythos::init::APP_CAP_START + i);
  }

  //res1 = myCS.deleteCap(res1.reuse(), example);
  //res1.wait();
  //ASSERT(res1.state() == mythos::Error::SUCCESS);
}

/**
 *
 *  Benchmark ping pong two threads
 *
 */
void* thread_ping_pong(void* ctx) {
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
}

void benchmark_ping_pong() {
  thread_manager mng(&thread_ping_pong);
  mythos::SchedulingCoordinator sc1(mythos::init::SCHEDULING_COORDINATOR_START + thread1);
  mythos::SchedulingCoordinator sc2(mythos::init::SCHEDULING_COORDINATOR_START + thread2);
  mythos::PortalLock pl(portal);

  ASSERT(sc1.setPolicy(pl, policy).wait());
  ASSERT(sc2.setPolicy(pl, policy).wait());
  pl.release();
  mng.create_thread(thread1, (void*)thread1, portal);
  mng.create_thread(thread2, (void*)thread2, portal);
  mythos::syscall_notify(mythos::init::APP_CAP_START + thread1);
}

void benchmarks() {
  //benchmark_wakeup_hwts();
  //benchmark_kernel_object_access();
  benchmark_ping_pong();
}

int main()
{
  char const begin[] = "Starting the benchmarks";
  char const end[] = "Benchmarks are done";
  //mythos::syscall_debug(begin, sizeof(begin) - 1);

  benchmarks();

  mythos::syscall_debug(end, sizeof(end) - 1);

  for (volatile uint64_t i = 0; i < 10000; i++) {
    for (volatile uint64_t ii = 0; ii < 100000; ii++) {
    }
  }

  return 0;
}