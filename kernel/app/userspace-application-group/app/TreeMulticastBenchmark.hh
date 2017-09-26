#pragma once

#include "runtime/IdleManagement.hh"
#include "app/TreeCombining.hh"

extern ThreadManager manager;
extern std::atomic<uint64_t> counter;
extern mythos::TreeCombining<NUM_THREADS,5> tc;

class TreeMulticastBenchmark {
public:
	TreeMulticastBenchmark(mythos::Portal &portal_)
		:portal(portal_) {}
	void test_multicast();
	void test_multicast_polling();
	void test_multicast_no_deep_sleep();
	void test_multicast_always_deep_sleep();
	void test_multicast_gen(uint64_t);
  void setup();
  void setup_polling();
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

void TreeMulticastBenchmark::setup_polling() {
  tc.init(NUM_THREADS - 4);
	manager.init([](void *data) -> void* {
		ASSERT(data != nullptr);
		auto *thread = reinterpret_cast<Thread*>(data);
    if (thread->id >= 4) {
      tc.dec(thread->id - 4);
    }
    while (true) {
      //MLOG_ERROR(mlog::app, "Thread loop", DVAR(thread->id));
      while (!thread->taskQueue.empty()) {
        auto *t = thread->taskQueue.pull();
        t->get()->run();
      }
      if (thread->SIGNALLED.exchange(false)) {
        if (thread->id >= 4) {
          tc.dec(thread->id - 4);
        }
      }
    }
	});
	manager.startAll();
  while(not tc.isFinished()) {}
}
void TreeMulticastBenchmark::setup() {
  tc.init(NUM_THREADS - 4);
	manager.init([](void *data) -> void* {
		ASSERT(data != nullptr);
		auto *thread = reinterpret_cast<Thread*>(data);
    if (thread->id >= 4) {
      tc.dec(thread->id - 4);
    }
	});
	manager.startAll();
  while(not tc.isFinished()) {}
}

void TreeMulticastBenchmark::test_multicast() {

  //setup();
  //test_multicast_no_deep_sleep();
	//test_multicast_always_deep_sleep();

  setup_polling();
  test_multicast_polling();
}

void TreeMulticastBenchmark::test_multicast_polling() {
	mythos::PortalLock pl(portal);
	for (uint64_t i = 4; i < manager.getNumThreads(); i++) {
		mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
		ASSERT(im.setPollingDelay(pl, (uint32_t)(-1)).wait()); // polling
		ASSERT(im.setLiteSleepDelay(pl, (uint32_t)(-1)).wait()); // max delay == no deep sleep
    manager.getThread(i)->signal();
	}
	pl.release();
	MLOG_ERROR(mlog::app, "Start tree polling  Signalable Group Test", DVAR(REPETITIONS));

  MLOG_CSV(mlog::app, "SignalGroup Size", "Cycles");
	mythos::delay(4000000);

	for (uint64_t i = 2; i < 5; i++) {
		test_multicast_gen(i);
	}
	for (uint64_t i = 5; i < manager.getNumThreads(); i+=5) {
		test_multicast_gen(i);
	}
  /*
  for (auto i = manager.getNumThreads() - 5; i >= 5 && i <=manager.getNumThreads(); i-=5) {
    test_multicast_gen(i);
  }
*/
	MLOG_ERROR(mlog::app, "End Polling Signalable Group Test");
}

void TreeMulticastBenchmark::test_multicast_no_deep_sleep() {
	mythos::PortalLock pl(portal);
	for (uint64_t i = 4; i < manager.getNumThreads(); i++) {
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
  for (auto i = manager.getNumThreads() - 5; i >= 5 && i <=manager.getNumThreads(); i-=5) {
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
		ASSERT(im.setLiteSleepDelay(pl, 0).wait()); // 0 == always deep sleep
    //manager.getThread(i)->signal();
	}
	pl.release();
	mythos::delay(400000);

	for (uint64_t i = 2; i < 5; i++) {
		test_multicast_gen(i);
	}
	for (uint64_t i = 5; i < manager.getNumThreads(); i+=5) {
		test_multicast_gen(i);
	}
  /*
  for (auto i = manager.getNumThreads() - 5; i >= 5 && i <=manager.getNumThreads(); i-=5) {
    test_multicast_gen(i);
  }
  */
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
    tc.init(number);
    t.start();
    group.signalAll();
    while(not tc.isFinished()) {}
    sum += t.end();
    mythos::delay(200000);
  }
	MLOG_CSV(mlog::app, number,  sum/REPETITIONS);

}
