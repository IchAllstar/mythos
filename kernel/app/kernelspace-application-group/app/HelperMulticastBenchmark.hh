#pragma once
#include <atomic>
#include "app/ThreadManager.hh"
#include "app/Thread.hh"
#include "runtime/SignalableGroup.hh"
#include "runtime/SimpleCapAlloc.hh"
#include "util/Time.hh"
#include "runtime/HelperThreadManager.hh"
#include "mythos/init.hh"


extern ThreadManager manager;
extern mythos::SimpleCapAllocDel caps;
extern mythos::KernelMemory kmem;
extern std::atomic<uint64_t> counter;
extern uint64_t repetitions;

class HelperMulticastBenchmark {
public:
    HelperMulticastBenchmark(mythos::Portal &pl_)
        : portal(pl_) {}
public:
    void setup();
    void test_multicast();
    void test_multicast_no_deep_sleep();
    void test_multicast_always_deep_sleep();
    void test_multicast_gen(uint64_t);

private:
    static const uint64_t numHelper = 15;
    static const uint64_t HELPER = 2; // identifier for setStrategy of SignalGroup
    mythos::Portal &portal;
    mythos::HelperThreadManager htm {mythos::init::HELPER_THREAD_MANAGER};
};

void HelperMulticastBenchmark::setup() {
    manager.init([](void *data) -> void* {
        counter.fetch_add(1);
    });
    manager.startAll();
    while (counter.load() != manager.getNumThreads() - 1) {}
    counter.store(0);
    mythos::PortalLock pl(portal);
    // register helper threads
    for (uint64_t i = 0; i < numHelper; i++) {
        htm.registerHelper(pl, manager.getNumThreads() - i - 1);
    }
}

void HelperMulticastBenchmark::test_multicast() {
  setup();
  test_multicast_no_deep_sleep();
  test_multicast_always_deep_sleep();
}

void HelperMulticastBenchmark::test_multicast_always_deep_sleep() {
    uint64_t num = numHelper; // else macro will result in undefined reference
    MLOG_ERROR(mlog::app, "Start Multicast Helper test with", num, "helpers and always deep sleep");
    mythos::PortalLock pl(portal);
    for (uint64_t i = 1; i < manager.getNumThreads() - numHelper; i++) {
      mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
      ASSERT(im.setPollingDelay(pl, 0).wait());
      ASSERT(im.setLiteSleepDelay(pl, 0).wait());
    }
    pl.release();

    // we have to wake up all used threads, so they can enter the configured
    // sleep state
    for (uint64_t i = 1; i < manager.getNumThreads(); i++) {
      manager.getThread(i)->signal();
    }
    mythos::delay(10000000);

    for (uint64_t i = 2; i < 5; i++) {
        test_multicast_gen(i);
    }
    for (uint64_t i = 5; i <= manager.getNumThreads() - numHelper; i += 5) {
        test_multicast_gen(i);
    }
    MLOG_ERROR(mlog::app, "End Multicast Helper test");
}

void HelperMulticastBenchmark::test_multicast_no_deep_sleep() {
    uint64_t num = numHelper; // else macro will result in undefined reference
    MLOG_ERROR(mlog::app, "Start Multicast Helper test with", num, "helpers and no deep sleep");

    mythos::PortalLock pl(portal);
    for (uint64_t i = 1; i < manager.getNumThreads() - numHelper; i++) {
      mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
      ASSERT(im.setPollingDelay(pl, 0).wait());
      ASSERT(im.setLiteSleepDelay(pl, (uint32_t(-1))).wait());
    }
    pl.release();
    // we have to wake up all used threads, so they can enter the configured
    // sleep state
    for (uint64_t i = 1; i < manager.getNumThreads(); i++) {
      manager.getThread(i)->signal();
    }
    mythos::delay(10000000);

    for (uint64_t i = 2; i < 5; i++) {
        test_multicast_gen(i);
    }
    for (uint64_t i = 5; i <= manager.getNumThreads() - numHelper; i += 5) {
        test_multicast_gen(i);
    }
    MLOG_ERROR(mlog::app, "End Multicast Helper test");
}

void HelperMulticastBenchmark::test_multicast_gen(uint64_t numThreads) {
    if (numThreads + 4 > manager.getNumThreads()) {
      MLOG_ERROR(mlog::app, "Cannot test with", DVAR(numThreads));
      return;
    }

    mythos::PortalLock pl(portal);
    mythos::SignalableGroup group(caps());
    ASSERT(group.create(pl, kmem, numThreads).wait());
    ASSERT(group.setCastStrategy(pl, HELPER));
    for (int i = 4; i < numThreads + 4; i++) {
        group.addMember(pl, manager.getThread(i)->ec).wait();
    }
    uint64_t sum = 0;
    mythos::Timer t;
    for (int j = 0; j < repetitions; j++) {
      counter.store(0);
      t.start();
      group.signalAll(pl);
      while (counter.load() != numThreads) { mythos::hwthread_pause(); }
      sum += t.end();
      mythos::delay(6000000); // wait to let threads enter deep sleep
    }

    MLOG_ERROR(mlog::app, DVAR(numThreads), DVAR(sum/repetitions));
    caps.free(group, pl);
    //manager.cleanup();
}
