#pragma once

#include "runtime/IdleManagement.hh"

extern ThreadManager manager;
extern std::atomic<uint64_t> counter;

class TreeMulticastBenchmark {
public:
	TreeMulticastBenchmark(mythos::Portal &portal_)
		:portal(portal_) {}
	void test_multicast();
	void test_multicast_no_deep_sleep();
	void test_multicast_always_deep_sleep();
	void test_multicast_gen(uint64_t);
  void setup();
private:
	void cleanup() {
		for (uint64_t i = 0; i < NUM_THREADS; ++i) {
			auto *t = manager.getThread(i);
			ASSERT(t);
			manager.deleteThread(*t);
		}
	}
private:
	mythos::Portal &portal;
};

void TreeMulticastBenchmark::setup() {
	manager.init([](void *data) -> void* {
		ASSERT(data != nullptr);
		auto *thread = reinterpret_cast<Thread*>(data);
		counter.fetch_add(1);
	});
	manager.startAll();


	// wait until all initialized
	while (counter.load() < manager.getNumThreads() - 1) {
		mythos::hwthread_pause(10);
	};
}

void TreeMulticastBenchmark::test_multicast() {
	setup();
  test_multicast_no_deep_sleep();
	test_multicast_always_deep_sleep();
}

void TreeMulticastBenchmark::test_multicast_no_deep_sleep() {
	mythos::PortalLock pl(portal);
	for (uint64_t i = 5; i < manager.getNumThreads(); i++) {
		mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
		ASSERT(im.setPollingDelay(pl, 0).wait());
		ASSERT(im.setLiteSleepDelay(pl, (uint32_t)(-1)).wait()); // max delay == no deep sleep
	}
	pl.release();
	MLOG_ERROR(mlog::app, "Start Tree No Deep Sleep Signalable Group Test", DVAR(REPETITIONS));

	mythos::delay(4000000);

	for (uint64_t i = 2; i < 5; i++) {
		test_multicast_gen(i);
	}
	for (uint64_t i = 5; i < manager.getNumThreads(); i+=5) {
		test_multicast_gen(i);
	}
	/*
  for (uint64_t i = manager.getNumThreads() - 5; i >= 10; i-=5) {
		test_multicast_gen(i);
	}
  */
	MLOG_ERROR(mlog::app, "End No Deep Sleep Signalable Group Test");
}

void TreeMulticastBenchmark::test_multicast_always_deep_sleep() {
	MLOG_ERROR(mlog::app, "Start always Deep Sleep Signalable Group Test", DVAR(REPETITIONS));
	mythos::PortalLock pl(portal);
	for (uint64_t i = 5; i < manager.getNumThreads(); i++) {
		mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
		ASSERT(im.setPollingDelay(pl, 0).wait());
		ASSERT(im.setLiteSleepDelay(pl, 0).wait()); // max delay == no deep sleep
	}
	pl.release();
	mythos::delay(4000000);

	for (uint64_t i = 2; i < 5; i++) {
		test_multicast_gen(i);
	}
	for (uint64_t i = 5; i < manager.getNumThreads(); i+=5) {
		test_multicast_gen(i);
	}

	MLOG_ERROR(mlog::app, "End always Deep Sleep Signalable Group Test");
}


void TreeMulticastBenchmark::test_multicast_gen(uint64_t number) {
	ASSERT(number < manager.getNumThreads() - 4);
	SignalGroup group;
  group.setStrat(SignalGroup::TREE);
	for (int i = 4; i < number + 4; ++i) {
		group.addMember(manager.getThread(i));
	}

  mythos::Timer t;
  uint64_t sum = 0;
  for (uint64_t i = 0; i < REPETITIONS; i++) {
    counter.store(0);
    t.start();
    group.signalAll();
    static uint64_t last_count = counter.load();
    while (counter.load() < number) {
      if (last_count != counter.load()) {
        last_count = counter.load();
        //MLOG_ERROR(mlog::app, DVAR(last_count));
      }
      mythos::hwthread_pause(10);
    }
    sum += t.end();
    ASSERT(counter.load() == number);
  }
	MLOG_ERROR(mlog::app, DVAR(number),  sum/REPETITIONS);
}


