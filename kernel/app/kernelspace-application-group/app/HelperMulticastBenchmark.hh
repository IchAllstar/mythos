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
    static const uint64_t numHelper = 3;
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
    for (uint64_t i = 0; i < numHelper; i++) {
        htm.registerHelper(pl, manager.getNumThreads() - i - 1);
    }
}

void HelperMulticastBenchmark::test_multicast() {
    MLOG_ERROR(mlog::app, "Start Multicast Helper test");
    setup();
    for (uint64_t i = 5; i <= 55; i += 5) {
        test_multicast_gen(i);
    }
    MLOG_ERROR(mlog::app, "ENd Multicast Helper test");
}

void HelperMulticastBenchmark::test_multicast_gen(uint64_t numThreads) {
    mythos::PortalLock pl(portal);
    mythos::SignalableGroup group(caps());
    ASSERT(group.create(pl, kmem, numThreads).wait());
    for (int i = 1; i < numThreads + 1; i++) {
        group.addMember(pl, manager.getThread(i)->ec).wait();
    }
    counter.store(0);
    mythos::Timer t;
    t.start();
    group.signalAll(pl).wait();
    while (counter.load() != numThreads) { mythos::hwthread_pause(); }
    MLOG_ERROR(mlog::app, DVAR(numThreads), DVAR(t.end()));
    caps.free(group, pl);
    //manager.cleanup();
}
