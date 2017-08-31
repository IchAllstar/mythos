#pragma once
#include "app/ThreadManager.hh"
#include "runtime/IdleManagement.hh"
#include <atomic>

extern ThreadManager manager;
extern std::atomic<uint64_t> counter;
/*
class HelperThreadBenchmark {
public:
	void init_worker() {
		for (uint64_t i = 1; i < NUM_THREADS; i++) {
			if (HelperThread::contains(i)) continue;

			manager.initThread(i, [](void *data) -> void* {
				auto *thread = reinterpret_cast<Thread*>(data);
				counter.fetch_add(1);
			});
			manager.startThread(*manager.getThread(i));
		}
	}

	void test_helper() {
		HelperThread::init_helper();
    MLOG_ERROR(mlog::app, "Init WOrker");
		init_worker();
		SignalGroup group;
		for (int i = 1; i < NUM_THREADS; i++) {
			if (HelperThread::contains(i)) continue;
			group.addMember(manager.getThread(i));
		}
		counter.store(0);
		uint64_t start, middle, end;
		start = mythos::getTime();
		group.signalAll();
		middle = mythos::getTime();
		while (counter.load(std::memory_order_relaxed) < NUM_THREADS - NUM_HELPER - 1) {
			mythos::hwthread_pause();
		}
		end = mythos::getTime();
		MLOG_ERROR(mlog::app, "Num Threads:", group.count(), "Helper:", NUM_HELPER, "Time:", end - start, "Middle", middle - start);
	}

};

*/

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
	mythos::delay(4000000);
}

void HelperThreadBenchmark::test_multicast() {
	init();
	test_multicast_no_deep_sleep();
	test_multicast_always_deep_sleep();
}

void HelperThreadBenchmark::test_multicast_no_deep_sleep() {
	mythos::PortalLock pl(portal);
	for (uint64_t i = 5; i < manager.getNumThreads() - NUM_HELPER; i++) {
		mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
		ASSERT(im.setPollingDelay(pl, 0).wait());
		ASSERT(im.setLiteSleepDelay(pl, (uint32_t)(-1)).wait()); // max delay == no deep sleep
    manager.getThread(i)->signal();
	}
	pl.release();
	mythos::delay(4000000);

	for (uint64_t i = 2; i < 5; i ++) {
		test_multicast_gen(i);
	}
	for (uint64_t i = 5; i < manager.getNumThreads() - NUM_HELPER; i += 5) {
		test_multicast_gen(i);
	}
	MLOG_ERROR(mlog::app, "End No Deep Sleep Signalable Group Test");
}

void HelperThreadBenchmark::test_multicast_always_deep_sleep() {
	MLOG_ERROR(mlog::app, "Start always Deep Sleep Signalable Group Test");
	mythos::PortalLock pl(portal);
	for (uint64_t i = 5; i < manager.getNumThreads() - NUM_HELPER; i++) {
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
	for (uint64_t i = 5; i < manager.getNumThreads() - NUM_HELPER; i += 5) {
		test_multicast_gen(i);
	}
	MLOG_ERROR(mlog::app, "End always Deep Sleep Signalable Group Test");
}


void HelperThreadBenchmark::test_multicast_gen(uint64_t number) {
	ASSERT(number < manager.getNumThreads() + 4);
	SignalGroup group;
	group.setStrat(SignalGroup::HELPER);
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
		while (counter.load() < number) {}
		sum += t.end();
    mythos::delay(400000);
	}
	MLOG_ERROR(mlog::app, DVAR(number),  sum / REPETITIONS);
}


