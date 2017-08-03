#pragma once

extern ThreadManager manager;
extern std::atomic<uint64_t> counter;

class TreeMulticastBenchmark {
public:
	void test_multicast() {
		manager.init([](void *data) -> void* {
			ASSERT(data != nullptr);
			auto *thread = reinterpret_cast<Thread*>(data);
			counter.fetch_add(1);
		});
		manager.startAll();

		// wait until all initialized
		while (counter.load() < NUM_THREADS - 1) {
			mythos::hwthread_pause(100);
		};

		MLOG_ERROR(mlog::app, "Signalable Group Test");
		SignalableGroup<100> group;
		//SignalableGroup<100, TreeStrategy> group;
		for (int i = 1; i < manager.getNumThreads(); ++i) {
			group.addMember(manager.getThread(i));
		}

		counter.store(0);

		uint64_t start , end;
		start = mythos::getTime();
		group.signalAll();
		while (counter.load() < NUM_THREADS - 1) {
			mythos::hwthread_pause(100);
		}

		end = mythos::getTime();
		MLOG_ERROR(mlog::app, DVAR(group.count()),  DVAR(end - start));
	}
};