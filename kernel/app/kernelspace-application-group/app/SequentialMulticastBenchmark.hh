#pragma once
#include "app/ThreadManager.hh"
#include "app/Thread.hh"
#include "runtime/SignalGroup.hh"
#include "runtime/SimpleCapAlloc.hh"
#include <atomic>
#include "util/Time.hh"


extern ThreadManager manager;
extern mythos::SimpleCapAllocDel caps;
extern mythos::KernelMemory kmem;
extern std::atomic<uint64_t> counter;
//extern uint64_t REPETITIONS;
extern mythos::TreeCombining<NUM_THREADS, 5> tc;

class SequentialMulticastBenchmark {
  public:
    SequentialMulticastBenchmark(mythos::Portal &pl_)
      : portal(pl_) {}
  public:
    void setup();
    void test_multicast();
    void test_single_thread();
    void test_multicast_gen(uint64_t);
    void test_multicast_no_deep_sleep();
    void test_multicast_polling();
    void test_multicast_always_deep_sleep();

  private:
    mythos::Portal &portal;
    static const constexpr uint64_t SEQUENTIAL = 0;
};

void SequentialMulticastBenchmark::setup() {
  manager.init([](void *data) -> void* {
        Thread* t = (Thread*) data;
        if (t->id >= 4) tc.dec(t->id - 4);
      });
  manager.startAll();
}

void SequentialMulticastBenchmark::test_multicast() {
  setup();
  //test_single_thread();
  test_multicast_no_deep_sleep();
  test_multicast_always_deep_sleep();
  //test_multicast_polling();
}

void SequentialMulticastBenchmark::test_single_thread() {
  {
    mythos::PortalLock pl(portal);
    mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + 4);
    ASSERT(im.setPollingDelay(pl, 0).wait());
    ASSERT(im.setLiteSleepDelay(pl,(uint32_t(-1))).wait());
    manager.getThread(4)->signal();
    mythos::delay(100000);
  }
  mythos::Timer t;
  uint64_t sum = 0;
  for (uint64_t i = 0; i < REPETITIONS; i++) {
    tc.init(1);
    t.start();
    manager.getThread(4)->signal();
    while(not tc.isFinished()) {}
    sum += t.end();
  }
  MLOG_ERROR(mlog::app, "Single Thread signal+acknowledge with lite sleep", sum / REPETITIONS);
  {
    mythos::PortalLock pl(portal);
    mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + 4);
    ASSERT(im.setPollingDelay(pl, (uint32_t(-1))).wait());
    ASSERT(im.setLiteSleepDelay(pl,(uint32_t(-1))).wait());
    manager.getThread(4)->signal();
    mythos::delay(100000);
  }
  sum = 0;
  for (uint64_t i = 0; i < REPETITIONS; i++) {
    tc.init(1);
    t.start();
    manager.getThread(4)->signal();
    while(not tc.isFinished()) {}
    sum += t.end();
  }
  MLOG_ERROR(mlog::app, "Single Thread signal+acknowledge with polling", sum / REPETITIONS);

  mythos::SignalGroup sg(caps());
  mythos::PortalLock pl(portal);
  ASSERT(sg.create(pl, kmem, 1).wait());
  ASSERT(sg.addMember(pl, manager.getThread(4)->ec).wait());
  sum = 0;
  for (uint64_t i = 0; i < REPETITIONS; i++) {
    tc.init(1);
    t.start();
    sg.signalAll(pl).wait();
    while(not tc.isFinished()) {}
    sum += t.end();
  }
  MLOG_ERROR(mlog::app, "Single Thread polling wrapped in SignalGroup", sum / REPETITIONS);
}

void SequentialMulticastBenchmark::test_multicast_always_deep_sleep() {
  MLOG_ERROR(mlog::app, "Start Sequential Multicast test always deep sleep");
  mythos::PortalLock pl(portal);
  tc.init(manager.getNumThreads() - 4);
  for (uint64_t i = 4; i < manager.getNumThreads(); i++) {
    mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
    ASSERT(im.setPollingDelay(pl, 0).wait());
    ASSERT(im.setLiteSleepDelay(pl, 0).wait());
    manager.getThread(i)->signal();
  }
  pl.release();
  mythos::delay(1000000);
  while (not tc.isFinished()) {}
  MLOG_CSV(mlog::app, "SignalGroup Size", "Cycles");
  for (uint64_t i = 2; i < 5; i++) { // does start with i+4th HWT
    test_multicast_gen(i);
  }
  for (uint64_t i = 5; i < manager.getNumThreads(); i += 5) {
    test_multicast_gen(i);
  }
  MLOG_ERROR(mlog::app, "End Sequential Multicast test");
}

void SequentialMulticastBenchmark::test_multicast_no_deep_sleep() {
  MLOG_ERROR(mlog::app, "Start Sequential Multicast test no deep sleep");
  mythos::PortalLock pl(portal);
  tc.init(manager.getNumThreads());
  for (uint64_t i = 0; i < manager.getNumThreads(); i++) {
    mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
    ASSERT(im.setPollingDelay(pl, 0).wait());
    ASSERT(im.setLiteSleepDelay(pl, (uint32_t(-1))).wait());
    manager.getThread(i)->signal();
  }
  pl.release();
  mythos::delay(10000000);
  MLOG_CSV(mlog::app, "SignalGroup Size", "Cycles");
  for (uint64_t i = 2; i < 5; i++) {
    test_multicast_gen(i);
  }
  for (uint64_t i = 5; i < manager.getNumThreads(); i += 5) {
    test_multicast_gen(i);
  }
  MLOG_ERROR(mlog::app, "End Sequential Multicast test");
}

void SequentialMulticastBenchmark::test_multicast_polling() {
  MLOG_ERROR(mlog::app, "Start Sequential Multicast test polling");
  mythos::PortalLock pl(portal);
  tc.init(manager.getNumThreads());
  for (uint64_t i = 0; i < manager.getNumThreads(); i++) {
    mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
    ASSERT(im.setPollingDelay(pl, (uint32_t(-1))).wait());
    ASSERT(im.setLiteSleepDelay(pl, (uint32_t(-1))).wait());
    manager.getThread(i)->signal();
  }
  pl.release();
  mythos::delay(10000000);
  MLOG_CSV(mlog::app, "SignalGroup Size", "Cycles");
  for (uint64_t i = 1; i < 5; i++) {
    test_multicast_gen(i);
  }
  for (uint64_t i = 5; i < manager.getNumThreads(); i += 5) {
    test_multicast_gen(i);
  }
  MLOG_ERROR(mlog::app, "End Sequential Multicast test");

}

void SequentialMulticastBenchmark::test_multicast_gen(uint64_t numThreads) {
  if (numThreads + 4 >= manager.getNumThreads()) {
    MLOG_ERROR(mlog::app, "Cannot test with", DVAR(numThreads));
  }
  mythos::PortalLock pl(portal);
  mythos::SignalGroup group(caps());
  ASSERT(group.create(pl, kmem, numThreads).wait());
  ASSERT(group.setCastStrategy(pl, SEQUENTIAL).wait());
  for (int i = 4; i < numThreads + 4; i++) {
    group.addMember(pl, manager.getThread(i)->ec).wait();
  }
  mythos::Timer t;
  uint64_t sum = 0;
  uint64_t min = (uint64_t)-1;
  for (uint64_t i = 0; i < REPETITIONS; i++) {
    tc.init(numThreads);
    t.start();
    group.signalAll(pl).wait();
    while(not tc.isFinished()) {}
    auto val = t.end();
    sum += val;
    if (min > val) min = val;
    mythos::delay(100000);
  }
  MLOG_CSV(mlog::app, numThreads, sum / REPETITIONS);
  caps.free(group, pl);
}
