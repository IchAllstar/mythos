#pragma once

extern ThreadManager manager;
extern std::atomic<uint64_t> counter;

class TreeMulticastBenchmark {
public:
	void test_multicast();

private:
	void cleanup() {
		for (uint64_t i = 0; i < NUM_THREADS; ++i) {
			auto *t = manager.getThread(i);
			ASSERT(t);
			manager.deleteThread(*t);
		}
	}
};

void TreeMulticastBenchmark::test_multicast() {
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

    for (volatile uint64_t i=0; i < 1000;i++) {
      for (volatile uint64_t j=0; j<100000;j++) {}
    }


		MLOG_ERROR(mlog::app, "Signalable Group Test");
		//SignalableGroup<100> group;
    	static auto threads = 3;
		SignalableGroup group;
		for (int i = 1; i < threads + 1; ++i) {
			group.addMember(manager.getThread(i));
		}

		counter.store(0);

		uint64_t start , end;
		start = mythos::getTime();
		group.signalAll();
    static uint64_t last_count = counter.load();
		while (counter.load() < threads) {
      if (last_count != counter.load()) {
        last_count = counter.load();
        //MLOG_ERROR(mlog::app, DVAR(last_count));
      }
			mythos::hwthread_pause(10);
		}

		end = mythos::getTime();
		MLOG_ERROR(mlog::app, DVAR(counter.load()), DVAR(group.count()),  DVAR(end - start));
	}
