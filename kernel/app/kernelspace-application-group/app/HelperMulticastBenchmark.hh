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

class HelperMulticastBenchmark {
public:
    HelperMulticastBenchmark(mythos::Portal &pl_)
        : portal(pl_) {}
public:
    void setup();
    void test_multicast();
    void test_multicast_gen(uint64_t);

private:
    static const uint64_t numHelper = 15;
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
    MLOG_ERROR(mlog::app, "Start Multicast Helper test");
    setup();
    uint64_t average = 0;
    for (uint64_t i = 5; i <= 200; i += 5) {
        test_multicast_gen(i);
        mythos::hwthread_pause(400000);
    }
    MLOG_ERROR(mlog::app, "End Multicast Helper test");
}

extern uint64_t repetitions;
void HelperMulticastBenchmark::test_multicast_gen(uint64_t numThreads) {
    mythos::PortalLock pl(portal);
    mythos::SignalableGroup group(caps());
    ASSERT(group.create(pl, kmem, numThreads).wait());
    for (int i = 1; i < numThreads + 1; i++) {
        group.addMember(pl, manager.getThread(i)->ec).wait();
    }
    uint64_t sum = 0;
    mythos::Timer t;
    for (int j = 0; j < repetitions; j++) {
      counter.store(0);
      t.start();
      group.signalAll(pl);
      while (counter.load() != numThreads) { /*mythos::hwthread_pause();*/ }
      sum += t.end();
    }

    MLOG_ERROR(mlog::app, DVAR(numThreads), DVAR(sum/repetitions));
    caps.free(group, pl);
    //manager.cleanup();
}
