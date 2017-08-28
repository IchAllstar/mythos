#pragma once
#include "app/ThreadManager.hh"
#include "app/Thread.hh"
#include "runtime/SignalableGroup.hh"
#include "runtime/SimpleCapAlloc.hh"
#include <atomic>
#include "util/Time.hh"


extern ThreadManager manager;
extern mythos::SimpleCapAllocDel caps;
extern mythos::KernelMemory kmem;
extern std::atomic<uint64_t> counter;
extern uint64_t REPETITIONS;

class TreeMulticastBenchmark {
public:
	TreeMulticastBenchmark(mythos::Portal &pl_)
		: portal(pl_) {}
public:
	void setup();
	void test_multicast();
  void test_multicast_no_deep_sleep();
  void test_multicast_always_deep_sleep();
	void test_multicast_gen(uint64_t);

private:
	mythos::Portal &portal;
  static const constexpr uint64_t TREE = 1;
};

void TreeMulticastBenchmark::setup() {
	manager.init([](void *data) -> void* {
		counter.fetch_add(1);
		//MLOG_ERROR(mlog::app, "Here");
	});
	manager.startAll();
	while (counter.load() != manager.getNumThreads() - 1) {}
	counter.store(0);
}

void TreeMulticastBenchmark::test_multicast() {
	setup();
  test_multicast_no_deep_sleep();
  test_multicast_always_deep_sleep();
}

void TreeMulticastBenchmark::test_multicast_always_deep_sleep() {
	MLOG_ERROR(mlog::app, "Start Tree Multicast tree test always deep sleep");
  mythos::PortalLock pl(portal);
  for (uint64_t i = 0; i < manager.getNumThreads(); i++) {
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
	for (uint64_t i = 5; i < manager.getNumThreads(); i += 5) {
	  test_multicast_gen(i);
	}
	MLOG_ERROR(mlog::app, "End Tree Multicast always deep sleep tree test");
}

void TreeMulticastBenchmark::test_multicast_no_deep_sleep() {
	MLOG_ERROR(mlog::app, "Start Tree Multicast tree test no deep sleep");
  mythos::PortalLock pl(portal);
  for (uint64_t i = 0; i < manager.getNumThreads(); i++) {
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
	for (uint64_t i = 5; i < manager.getNumThreads(); i += 5) {
		test_multicast_gen(i);
	}
	MLOG_ERROR(mlog::app, "End Tree Multicast no deep sleep tree test");
}

void TreeMulticastBenchmark::test_multicast_gen(uint64_t numThreads) {
	if (numThreads + 4 >= manager.getNumThreads()) {
    MLOG_ERROR(mlog::app, "Cannot run test with", DVAR(numThreads));
    return;
  }
  mythos::PortalLock pl(portal);
	mythos::SignalableGroup group(caps());
	ASSERT(group.create(pl, kmem, numThreads).wait());
	ASSERT(group.setCastStrategy(pl, TREE));
  for (int i = 4; i < numThreads + 4; i++) {
		group.addMember(pl, manager.getThread(i)->ec).wait();
	}
	mythos::Timer t;
	uint64_t sum = 0;
	for (uint64_t i = 0; i < REPETITIONS; i++) {
		counter.store(0);
		t.start();
    group.signalAll(pl).wait();
		while (counter.load() != numThreads) { /*mythos::hwthread_pause();*/ }
		sum += t.end();
    mythos::delay(1000000); // Tree cast does lock when not here
	}
	MLOG_ERROR(mlog::app, DVAR(numThreads),"Avg. Cycles: ", sum / REPETITIONS);
	caps.free(group, pl);
}
