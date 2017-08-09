#pragma once
#include "app/ThreadManager.hh"
#include <atomic>

extern ThreadManager manager;
extern std::atomic<uint64_t> counter;
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
		init_worker();
		SignalableGroup group;
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