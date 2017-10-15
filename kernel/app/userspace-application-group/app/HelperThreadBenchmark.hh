#pragma once
#include "app/ThreadManager.hh"
#include "runtime/IdleManagement.hh"
#include <atomic>
#include "app/TreeCombining.hh"

extern ThreadManager manager;
extern std::atomic<uint64_t> counter;
extern mythos::TreeCombining<NUM_THREADS, 4> tc;

class HelperThreadBenchmark {
public:
	HelperThreadBenchmark(mythos::Portal &portal_)
		: portal(portal_) {}
	void test_multicast();
	void test_multicast_no_deep_sleep();
  void test_multicast_polling();
	void test_multicast_always_deep_sleep();
	void test_multicast_always_deep_sleep_helper();
  void test_multicast_different_helper();
	void test_multicast_gen(uint64_t);
  uint64_t test_multicast_ret(uint64_t number, uint64_t helper = NUM_HELPER);
	void init();
  void init_polling();

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
	});
	manager.startAll();
	// wait until all initialized
	while(not tc.isFinished()) {}
}

void HelperThreadBenchmark::init_polling() {
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
		while (true) {
      while (!thread->taskQueue.empty()) {
        auto *t = thread->taskQueue.pull();
        t->get()->run();
      }
      if (thread->SIGNALLED.exchange(false)) {
        if (thread->id >= 4 && thread->id < NUM_THREADS - NUM_HELPER) {
          tc.dec(thread->id - 4);
        }
      }
    }
	});
	manager.startAll();
	// wait until all initialized
	//while(not tc.isFinished()) {}
}

void HelperThreadBenchmark::test_multicast() {
	init();
	//test_multicast_no_deep_sleep();
	//test_multicast_always_deep_sleep();

  //test_multicast_always_deep_sleep_helper();
  test_multicast_different_helper();
  //init_polling();
  //test_multicast_polling();
}

void HelperThreadBenchmark::test_multicast_different_helper() {
    MLOG_ERROR(mlog::app, "Start Multicast Helper test with different numbers of helper");
    mythos::PortalLock pl(portal);
    tc.init(manager.getNumThreads() - NUM_HELPER - 4);
    for (uint64_t i = 4; i < manager.getNumThreads() - NUM_HELPER; i++) {
      mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
      ASSERT(im.setPollingDelay(pl, 0).wait());
      ASSERT(im.setLiteSleepDelay(pl, ((uint32_t)-1)).wait());
      manager.getThread(i)->signal();
    }
    pl.release();
    mythos::delay(1000000);
    while (not tc.isFinished()) {}

    MLOG_CSV(mlog::app,"GroupSize","Helper","Cycles");
    for (auto i = 5ul; i <= 215; i += 5) {
      uint64_t min = ((uint64_t)-1);
      uint64_t min_h =((uint64_t)-1);
      for (auto h = 2ul; h < NUM_HELPER; h++) {
        uint64_t cycles = test_multicast_ret(i, h);
        if (cycles < min) {
          min = cycles;
          min_h = h;
        }
      }
      //MLOG_ERROR(mlog::app,"GroupSize:", i, "Helper:", min_h, "Cycles", min);
      MLOG_CSV(mlog::app, i, min_h, min);
    }

    MLOG_ERROR(mlog::app, "End Multicast Helper test");
}

void HelperThreadBenchmark::test_multicast_always_deep_sleep_helper() {
    MLOG_ERROR(mlog::app, "Start Multicast Helper test with helpers and always deep sleep");
    mythos::PortalLock pl(portal);
    tc.init(manager.getNumThreads() - NUM_HELPER - 4);
    for (uint64_t i = 4; i < manager.getNumThreads() - NUM_HELPER; i++) {
      mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
      ASSERT(im.setPollingDelay(pl, 0).wait());
      ASSERT(im.setLiteSleepDelay(pl, 0).wait());
      manager.getThread(i)->signal();
    }
    pl.release();
    mythos::delay(10000000);
    while (not tc.isFinished()) {}
    uint64_t values_helper[NUM_HELPER][46];
    for (uint64_t helper = 2; helper < NUM_HELPER; helper++) {
      MLOG_ERROR(mlog::app, "Testing with", helper);
      auto run = 0ul;
      for (uint64_t i = 2; i < 5; i++) {
          values_helper[helper][run] = test_multicast_ret(i, helper);
          run++;
      }
      for (uint64_t i = 5; i <= 215; i += 5) {
          values_helper[helper][run] = test_multicast_ret(i, helper);
          run++;
      }
    }

    MLOG_CSV(mlog::app, "SignalGroup Size", "2", "3", "4", "5", "6", "7","8","9","10","11","12","13","14","15","16","17","18","19","20");
    auto run = 0ul;
    for (uint64_t i = 2; i < 5; i++) {
      MLOG_CSV(mlog::app,i,values_helper[2][run],values_helper[3][run],values_helper[4][run],values_helper[5][run],values_helper[6][run],values_helper[7][run],values_helper[8][run],values_helper[9][run],values_helper[10][run],values_helper[11][run],values_helper[12][run],values_helper[13][run],values_helper[14][run],values_helper[15][run],values_helper[16][run],values_helper[17][run],values_helper[18][run],values_helper[19][run],values_helper[20][run]);
      run++;
    }
    for (uint64_t i = 5; i <= manager.getNumThreads() - NUM_HELPER - 4; i += 5) {
      MLOG_CSV(mlog::app,i,values_helper[2][run],values_helper[3][run],values_helper[4][run],values_helper[5][run],values_helper[6][run],values_helper[7][run],values_helper[8][run],values_helper[9][run],values_helper[10][run],values_helper[11][run],values_helper[12][run],values_helper[13][run],values_helper[14][run],values_helper[15][run],values_helper[16][run],values_helper[17][run],values_helper[18][run],values_helper[19][run],values_helper[20][run]);
        run++;
    }
    MLOG_ERROR(mlog::app, "End Multicast Helper test");
}

void HelperThreadBenchmark::test_multicast_polling() {
	MLOG_ERROR(mlog::app, "Start polling group test");
  mythos::PortalLock pl(portal);
  tc.init(manager.getNumThreads() - NUM_HELPER - 4);
	for (uint64_t i = 4; i < manager.getNumThreads() - NUM_HELPER; i++) {
		mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
		ASSERT(im.setPollingDelay(pl, (uint32_t)(-1)).wait());
		ASSERT(im.setLiteSleepDelay(pl, (uint32_t)(-1)).wait()); // max delay == no deep sleep
    manager.getThread(i)->signal();
	}
	pl.release();
	mythos::delay(4000000);
  while(not tc.isFinished()) {}

	for (uint64_t i = 2; i < 5; i ++) {
		test_multicast_gen(i);
	}
	for (uint64_t i = 5; i < manager.getNumThreads() - NUM_HELPER - 4; i += 5) {
		test_multicast_gen(i);
	}
	MLOG_ERROR(mlog::app, "End No Deep Sleep Signalable Group Test");
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
  tc.init(manager.getNumThreads() - NUM_HELPER - 4);
	for (uint64_t i = 4; i < manager.getNumThreads() - NUM_HELPER; i++) {
		mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
		ASSERT(im.setPollingDelay(pl, 0).wait());
		ASSERT(im.setLiteSleepDelay(pl, 0).wait()); // max delay == no deep sleep
    manager.getThread(i)->signal();
	}
	pl.release();
  while(not tc.isFinished()) {}
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
	SignalGroup group;
	group.setStrat(SignalGroup::HELPER);
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
    mythos::delay(400000);
	}
	MLOG_CSV(mlog::app, number,  sum / REPETITIONS);
}

uint64_t HelperThreadBenchmark::test_multicast_ret(uint64_t number, uint64_t helper) {
	ASSERT(number <= manager.getNumThreads() - NUM_HELPER - 4);
	SignalGroup group;
	group.setStrat(SignalGroup::HELPER);
  group.setAvailableHelper(helper);
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
    mythos::delay(400000);
	}
  return sum / REPETITIONS;
}
