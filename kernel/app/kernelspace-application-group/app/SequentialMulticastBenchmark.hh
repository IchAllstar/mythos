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
  void test_multicast_always_deep_sleep();

private:
	mythos::Portal &portal;
  static const constexpr uint64_t SEQUENTIAL = 0;
};

void SequentialMulticastBenchmark::setup() {
	counter.store(0);
  manager.init([](void *data) -> void* {
		counter.fetch_add(1);
		//MLOG_ERROR(mlog::app, "Here");
	});
	manager.startAll();
	while (counter.load() != manager.getNumThreads() - 1) {}
	counter.store(0);
}

void SequentialMulticastBenchmark::test_multicast() {
	setup();
  test_single_thread();
  test_multicast_no_deep_sleep();
  test_multicast_always_deep_sleep();
}

void SequentialMulticastBenchmark::test_single_thread() {
  {
    mythos::PortalLock pl(portal);
    for (uint64_t i = 0; i < manager.getNumThreads(); i++) {
      mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
      ASSERT(im.setPollingDelay(pl, 0).wait());
      ASSERT(im.setLiteSleepDelay(pl,(uint32_t(-1))).wait());
      manager.getThread(i)->signal();
    }
    mythos::delay(100000);
  }
  MLOG_ERROR(mlog::app, "Test single Thread performance.");
  mythos::Timer t;
  uint64_t sum = 0;
  for (uint64_t i = 0; i < REPETITIONS; i++) {
    counter.store(0);
    t.start();
    manager.getThread(10)->signal();
    while (counter.load() == 0) {}
    sum += t.end();
  }
  MLOG_ERROR(mlog::app, "Single Thread with ThreadManager", sum / REPETITIONS);
}

void SequentialMulticastBenchmark::test_multicast_always_deep_sleep() {
	MLOG_ERROR(mlog::app, "Start Sequential Multicast tree test always deep sleep");
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
  for (uint64_t i = 2; i < 5; i++) { // does start with i+4th HWT
    test_multicast_gen(i);
  }
	for (uint64_t i = 5; i < manager.getNumThreads(); i += 5) {
	  test_multicast_gen(i);
	}
	MLOG_ERROR(mlog::app, "End Sequential Multicast tree test");
}

void SequentialMulticastBenchmark::test_multicast_no_deep_sleep() {
	MLOG_ERROR(mlog::app, "Start Sequential Multicast tree test no deep sleep");
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
	MLOG_ERROR(mlog::app, "End Sequential Multicast tree test");
}

void SequentialMulticastBenchmark::test_multicast_gen(uint64_t numThreads) {
  if (numThreads + 4 >= manager.getNumThreads()) {
    MLOG_ERROR(mlog::app, "Cannot test with", DVAR(numThreads));
  }
	mythos::PortalLock pl(portal);
	mythos::SignalableGroup group(caps());
	ASSERT(group.create(pl, kmem, numThreads).wait());
  ASSERT(group.setCastStrategy(pl, SEQUENTIAL).wait());
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
    mythos::delay(10000000);
  }
	MLOG_ERROR(mlog::app, numThreads,"; ", sum / REPETITIONS);
	caps.free(group, pl);
}
