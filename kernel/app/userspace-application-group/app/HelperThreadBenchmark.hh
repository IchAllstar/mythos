#pragma once
#include "app/ThreadManager.hh"
#include "runtime/IdleManagement.hh"
#include <atomic>
#include "app/TreeCombining.hh"

extern ThreadManager manager;
extern std::atomic<uint64_t> counter;
extern mythos::TreeCombining<NUM_THREADS, 5> tc;

class HelperThreadBenchmark {
public:
	HelperThreadBenchmark(mythos::Portal &portal_)
		: portal(portal_) {}
	void test_multicast();
	void test_multicast_no_deep_sleep();
	void test_multicast_always_deep_sleep();
	void test_multicast_gen(uint64_t);
	void init();

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

void HelperThreadBenchmark::init() {
	// init helper
	mythos::PortalLock pl(portal);
	for (uint64_t i = manager.getNumThreads() - NUM_HELPER; i < manager.getNumThreads(); i++) {
		mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
		ASSERT(im.setPollingDelay(pl, (uint32_t)(-1)).wait()); // max delay == polling only
		ASSERT(im.setLiteSleepDelay(pl, (uint32_t)(-1)).wait()); // max delay == no deep sleep
	}
	pl.release();
  tc.init(NUM_THREADS - 4 - NUM_HELPER);
	manager.init([](void *data) -> void* {
		auto *thread = reinterpret_cast<Thread*>(data);
    if (thread->id >= 4 && thread->id < NUM_THREADS - NUM_HELPER) {
      tc.dec(thread->id - 4);
    }
		//counter.fetch_add(1);
	});
	manager.startAll();
	// wait until all initialized
	//while (counter.load() < manager.getNumThreads() - 1) {};
	while(not tc.isFinished()) {}
}

void HelperThreadBenchmark::test_multicast() {
	init();
	test_multicast_no_deep_sleep();
	test_multicast_always_deep_sleep();
}

void HelperThreadBenchmark::test_multicast_no_deep_sleep() {
	MLOG_ERROR(mlog::app, "Start no deep sleep group test");
  mythos::PortalLock pl(portal);
	for (uint64_t i = 4; i < manager.getNumThreads() - NUM_HELPER; i++) {
		mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
		ASSERT(im.setPollingDelay(pl, 0).wait());
		ASSERT(im.setLiteSleepDelay(pl, (uint32_t)(-1)).wait()); // max delay == no deep sleep
    //manager.getThread(i)->signal();
	}
	pl.release();
	mythos::delay(4000000);

	for (uint64_t i = 2; i < 5; i ++) {
		test_multicast_gen(i);
	}
	for (uint64_t i = 5; i < manager.getNumThreads() - NUM_HELPER - 4; i += 5) {
		test_multicast_gen(i);
	}
	MLOG_ERROR(mlog::app, "End No Deep Sleep Signalable Group Test");
}

void HelperThreadBenchmark::test_multicast_always_deep_sleep() {
	MLOG_ERROR(mlog::app, "Start always Deep Sleep Signalable Group Test");
	mythos::PortalLock pl(portal);
	for (uint64_t i = 4; i < manager.getNumThreads() - NUM_HELPER; i++) {
		mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
		ASSERT(im.setPollingDelay(pl, 0).wait());
		ASSERT(im.setLiteSleepDelay(pl, 0).wait()); // max delay == no deep sleep
    //manager.getThread(i)->signal();
	}
	pl.release();
	mythos::delay(4000000);
	for (uint64_t i = 2; i < 5; i++) {
		test_multicast_gen(i);
	}
	for (uint64_t i = 5; i < manager.getNumThreads() - NUM_HELPER - 4; i += 5) {
		test_multicast_gen(i);
	}
	MLOG_ERROR(mlog::app, "End always Deep Sleep Signalable Group Test");
}


void HelperThreadBenchmark::test_multicast_gen(uint64_t number) {
	ASSERT(number < manager.getNumThreads() - NUM_HELPER - 4);
  MLOG_ERROR(mlog::app, "Try", number);
	SignalGroup group;
	group.setStrat(SignalGroup::HELPER);
	for (int i = 4; i < number + 4; ++i) {
		group.addMember(manager.getThread(i));
	}
	mythos::Timer t;
	uint64_t sum = 0;
	for (uint64_t i = 0; i < REPETITIONS; i++) {
		//counter.store(0);
    tc.init(number);
		t.start();
		group.signalAll();
		//while (counter.load() < number) {}
    while(not tc.isFinished()) {}
		sum += t.end();
    mythos::delay(400000);
	}
	MLOG_ERROR(mlog::app, DVAR(number),  sum / REPETITIONS);
}


