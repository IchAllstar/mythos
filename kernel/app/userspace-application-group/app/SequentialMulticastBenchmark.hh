#pragma once

#include "runtime/IdleManagement.hh"

extern ThreadManager manager;
extern std::atomic<uint64_t> counter;

class SequentialMulticastBenchmark {
public:
	SequentialMulticastBenchmark(mythos::Portal &portal_)
		:portal(portal_) {}
  void setup();
	void test_multicast();
	void test_signal_single_thread();
  void test_multicast_no_deep_sleep();
	void test_multicast_always_deep_sleep();
	void test_multicast_gen(uint64_t);

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

void SequentialMulticastBenchmark::setup() {
	manager.init([](void *data) -> void* {
		//ASSERT(data != nullptr);
		//auto *thread = reinterpret_cast<Thread*>(data);
		counter.fetch_add(1);
	});
	manager.startAll();
}

void SequentialMulticastBenchmark::test_multicast() {
  setup();
	test_signal_single_thread();
  test_multicast_no_deep_sleep();
	test_multicast_always_deep_sleep();
}

void SequentialMulticastBenchmark::test_signal_single_thread() {
	mythos::PortalLock pl(portal);
	for (uint64_t i = 5; i < manager.getNumThreads(); i++) {
		mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
		ASSERT(im.setPollingDelay(pl, 0).wait());
		ASSERT(im.setLiteSleepDelay(pl, (uint32_t)(-1)).wait()); // max delay == no deep sleep
    manager.getThread(i)->signal();
	}
	pl.release();
	MLOG_ERROR(mlog::app, "Start Single wakeup test", DVAR(REPETITIONS));
	mythos::delay(4000000);

  mythos::Timer t;
  uint64_t sum = 0;

  for (uint64_t thread = 1; thread < manager.getNumThreads(); thread++) {
    sum = 0;
    for (int i = 0; i < REPETITIONS; i++) {
      t.start();
      manager.getThread(thread)->signal();
      while(counter.load() == 0) {}
      sum += t.end();
      counter.store(0);
      mythos::hwthread_pause(1000);
    }
    MLOG_ERROR(mlog::app, "single thread:", thread, "; ", sum/REPETITIONS);
    mythos::hwthread_pause(10000);
  }

}

void SequentialMulticastBenchmark::test_multicast_no_deep_sleep() {
	mythos::PortalLock pl(portal);
	for (uint64_t i = 5; i < manager.getNumThreads(); i++) {
		mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
		ASSERT(im.setPollingDelay(pl, 0).wait());
		ASSERT(im.setLiteSleepDelay(pl, (uint32_t)(-1)).wait()); // max delay == no deep sleep
    manager.getThread(i)->signal();
	}
	pl.release();
	MLOG_ERROR(mlog::app, "Start No Deep Sleep Signalable Group Test", DVAR(REPETITIONS));

	mythos::delay(4000000);

	for (uint64_t i = 2; i < 5; i++) {
		test_multicast_gen(i);
	}
	for (uint64_t i = 5; i < manager.getNumThreads(); i+=5) {
		test_multicast_gen(i);
	}

	MLOG_ERROR(mlog::app, "End No Deep Sleep Signalable Group Test");
}

void SequentialMulticastBenchmark::test_multicast_always_deep_sleep() {
	MLOG_ERROR(mlog::app, "Start always Deep Sleep Signalable Group Test", DVAR(REPETITIONS));
	mythos::PortalLock pl(portal);
	for (uint64_t i = 5; i < manager.getNumThreads(); i++) {
		mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
		ASSERT(im.setPollingDelay(pl, 0).wait());
		ASSERT(im.setLiteSleepDelay(pl, 0).wait()); // max delay == no deep sleep
    manager.getThread(i)->signal();
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


void SequentialMulticastBenchmark::test_multicast_gen(uint64_t number) {
	ASSERT(number < manager.getNumThreads() + 4);
	SignalableGroup group;
  group.setStrat(SignalableGroup::SEQUENTIAL);
	for (int i = 4; i < number + 4; ++i) {
		group.addMember(manager.getThread(i));
	}
  counter.store(0);
  mythos::Timer t;
  uint64_t sum = 0;
  for (uint64_t i = 0; i < REPETITIONS; i++) {
    t.start();
    group.signalAll();
    static uint64_t last_count = counter.load();
    while (counter.load() < number) { }
    sum += t.end();
    counter.store(0);
    mythos::delay(100000);
  }
	MLOG_ERROR(mlog::app, DVAR(number),  sum/REPETITIONS);
}


