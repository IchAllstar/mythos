#pragma once
#include "app/ThreadManager.hh"
#include "app/Thread.hh"
#include "runtime/SignalGroup.hh"
#include "runtime/SimpleCapAlloc.hh"
#include <atomic>
#include "util/Time.hh"
#include "app/TreeCombining.hh"


extern ThreadManager manager;
extern mythos::SimpleCapAllocDel caps;
extern mythos::KernelMemory kmem;
extern mythos::TreeCombining<NUM_THREADS, 5> tc;

extern uint64_t values1[NUM_THREADS];
extern uint64_t values2[NUM_THREADS];
extern uint64_t values3[REPETITIONS];
extern uint64_t values4[REPETITIONS];

class TreeMulticastBenchmark {
public:
	TreeMulticastBenchmark(mythos::Portal &pl_)
		: portal(pl_) {}
public:
	void setup();
	void test_multicast();
  void test_multicast_no_deep_sleep();
  void test_multicast_always_deep_sleep();
  void test_multicast_max_size();
  void test_multicast_polling();
  void test_multicast_both_sleep();
	uint64_t test_multicast_gen(uint64_t number , uint64_t repetitions = REPETITIONS);

private:
	mythos::Portal &portal;
  static const constexpr uint64_t TREE = 1;


};

void TreeMulticastBenchmark::setup() {
	tc.init(manager.getNumThreads() - 4);
  manager.init([](void *data) -> void* {
		Thread *t = (Thread*) data;
    auto id = (uint64_t)t->id;
    if (id >= 4) tc.dec(id - 4);
		//if (id >= 4) MLOG_ERROR(mlog::app, DVAR(id));
	});
	manager.startAll();
  while(not tc.isFinished()) {}
}

void TreeMulticastBenchmark::test_multicast() {
    setup();
    MLOG_ERROR(mlog::app, "Tree Multicast Benchmark. May take a while to complete ...");


    //test_multicast_both_sleep();
    //test_multicast_max_size();
    test_multicast_polling();
    MLOG_ERROR(mlog::app, "End of all Benchmarks");
}

void TreeMulticastBenchmark::test_multicast_max_size() {
	//MLOG_ERROR(mlog::app, "Start Tree Multicast tree test no deep sleep");
  mythos::PortalLock pl(portal);
  for (uint64_t i = 4; i < manager.getNumThreads(); i++) {
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

  // the Lapic seems to need some "warmup"; going directly with 235 threads
  // leeds to lapic hanging on pending flag error
  for (uint64_t i = 5; i < manager.getNumThreads(); i += 5) {
		values1[i] = test_multicast_gen(i,1);
	}

  for (auto i = 0ul; i < REPETITIONS; i++) {
    values3[i] = test_multicast_gen(230,1);
  }


  //DEEP SLEEP
  mythos::PortalLock pl2(portal);
  for (uint64_t i = 4; i < manager.getNumThreads(); i++) {
    mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
    ASSERT(im.setPollingDelay(pl2, 0).wait());
    ASSERT(im.setLiteSleepDelay(pl2, 0).wait());
  }
  pl2.release();
  mythos::delay(10000000);

  for (auto i = 0ul; i < REPETITIONS; i++) {
    values4[i] = test_multicast_gen(230,1);
  }

  // output
  MLOG_CSV(mlog::app, "Run", "No Deep Sleep", "Deep Sleep");
  for (auto i = 0ul; i < REPETITIONS; i++) {
    MLOG_CSV(mlog::app, i, values3[i], values4[i]);
  }

	MLOG_ERROR(mlog::app, "End Tree Multicast no deep sleep tree test");
}

void TreeMulticastBenchmark::test_multicast_polling() {
  mythos::PortalLock pl(portal);
  for (uint64_t i = 4; i < manager.getNumThreads(); i++) {
    mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
    ASSERT(im.setPollingDelay(pl, (uint32_t(-1))).wait());
    ASSERT(im.setLiteSleepDelay(pl, (uint32_t(-1))).wait());
    manager.getThread(i)->signal();
  }
  pl.release();
  mythos::delay(10000000);
  MLOG_CSV(mlog::app, "SignalGroup Size", "Cycles");
  for (uint64_t i = 2; i < 5; i++) {
    MLOG_CSV(mlog::app, i, test_multicast_gen(i));
  }
	for (uint64_t i = 5; i < manager.getNumThreads(); i += 5) {
    MLOG_CSV(mlog::app, i, test_multicast_gen(i));
	}

	MLOG_ERROR(mlog::app, "End Tree Multicast no deep sleep tree test");
}

void TreeMulticastBenchmark::test_multicast_both_sleep() {
  test_multicast_no_deep_sleep();
  test_multicast_always_deep_sleep();
  MLOG_CSV(mlog::app, "SignalGroup Size", "Cycles no deep sleep", "Cycles deep sleep");
  for (auto i = 2ul; i < 5; i++) {
    MLOG_CSV(mlog::app, i, values1[i], values2[i]);
  }

  for (uint64_t i = 5; i < manager.getNumThreads(); i += 5) {
    MLOG_CSV(mlog::app, i, values1[i], values2[i]);
  }
}

void TreeMulticastBenchmark::test_multicast_always_deep_sleep() {
	MLOG_ERROR(mlog::app, "Start Tree Multicast tree test always deep sleep");
  mythos::PortalLock pl(portal);
  for (uint64_t i = 4; i < manager.getNumThreads(); i++) {
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
  //MLOG_CSV(mlog::app, "SignalGroup Size", "Cycles");
  for (uint64_t i = 2; i < 5; i++) {
    values2[i] = test_multicast_gen(i);
  }
	for (uint64_t i = 5; i < manager.getNumThreads(); i += 5) {
	  values2[i] = test_multicast_gen(i);
	}

	MLOG_ERROR(mlog::app, "End Tree Multicast always deep sleep tree test");
}

void TreeMulticastBenchmark::test_multicast_no_deep_sleep() {
	//MLOG_ERROR(mlog::app, "Start Tree Multicast tree test no deep sleep");
  mythos::PortalLock pl(portal);
  for (uint64_t i = 4; i < manager.getNumThreads(); i++) {
    mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
    ASSERT((im.setPollingDelay(pl, 0)).wait());
    ASSERT(im.setLiteSleepDelay(pl, (uint32_t(-1))).wait());
  }
  pl.release();
  // we have to wake up all used threads, so they can enter the configured
  // sleep state
  for (uint64_t i = 1; i < manager.getNumThreads(); i++) {
    manager.getThread(i)->signal();
  }
  mythos::delay(10000000);
  //MLOG_CSV(mlog::app, "SignalGroup Size", "Cycles");
  for (uint64_t i = 2; i < 5; i++) {
    values1[i] = test_multicast_gen(i);
  }
	for (uint64_t i = 5; i < manager.getNumThreads(); i += 5) {
		values1[i] = test_multicast_gen(i);
	}
	//MLOG_ERROR(mlog::app, "End Tree Multicast no deep sleep tree test");
}

uint64_t TreeMulticastBenchmark::test_multicast_gen(uint64_t numThreads, uint64_t repetitions) {
	if (numThreads + 4 >= manager.getNumThreads()) {
    MLOG_ERROR(mlog::app, "Cannot run test with", DVAR(numThreads));
    return 0;
  }

  mythos::PortalLock pl(portal);
	mythos::SignalGroup group(caps());
	ASSERT(group.create(pl, kmem, numThreads).wait());
	ASSERT(group.setCastStrategy(pl, TREE));
  for (int i = 4; i < numThreads + 4; i++) {
		ASSERT(group.addMember(pl, manager.getThread(i)->ec).wait());
	}

	mythos::Timer t;
	uint64_t sum = 0;
	for (uint64_t i = 0; i < repetitions; i++) {
    tc.init(numThreads);
		t.start();
    group.signalAll(pl).wait();
    while(not tc.isFinished()) {}
		sum += t.end();
    mythos::delay(1000000);
	}
	caps.free(group, pl);
  return sum / repetitions;
}
