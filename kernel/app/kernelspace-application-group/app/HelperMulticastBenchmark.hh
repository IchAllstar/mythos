#pragma once
#include <atomic>
#include "app/ThreadManager.hh"
#include "app/Thread.hh"
#include "runtime/SignalGroup.hh"
#include "runtime/SimpleCapAlloc.hh"
#include "util/Time.hh"
#include "mythos/init.hh"


extern ThreadManager manager;
extern mythos::SimpleCapAllocDel caps;
extern mythos::KernelMemory kmem;
extern std::atomic<uint64_t> counter;
//extern uint64_t REPETITIONS;
extern mythos::TreeCombining<NUM_THREADS, 5> tc;

class HelperMulticastBenchmark {
public:
    HelperMulticastBenchmark(mythos::Portal &pl_)
        : portal(pl_) {}
private:
    static const uint64_t numHelper = 21;
    static const uint64_t HELPER = 2; // identifier for setStrategy of SignalGroup
    mythos::Portal &portal;
public:
    void setup();
    void test_multicast();
    void test_multicast_no_deep_sleep();
    void test_multicast_always_deep_sleep();
    void test_multicast_always_deep_sleep_helper();
    void test_multicast_polling();
    void test_multicast_different_helper();
    void test_multicast_gen(uint64_t threads, uint64_t helper = numHelper);
    uint64_t test_multicast_ret(uint64_t threads, uint64_t helper = numHelper);
};

void HelperMulticastBenchmark::setup() {
    tc.init(manager.getNumThreads());
    manager.init([](void *data) -> void* {
        Thread *t = (Thread*) data;
        if (t->id >= 4 && t->id < manager.getNumThreads() - numHelper) {
          tc.dec(t->id - 4);
        }
    });
    manager.startAll();

    mythos::PortalLock pl(portal);
    for (uint64_t i = 0; i < numHelper; i++) {
        auto helper = manager.getNumThreads() - i - 1;
        mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + helper);
        ASSERT(im.setPollingDelay(pl, (uint32_t(-1))).wait());
        ASSERT(im.setLiteSleepDelay(pl, (uint32_t(-1))).wait());
    }
}

void HelperMulticastBenchmark::test_multicast() {
  setup();
  //test_multicast_no_deep_sleep();
  //test_multicast_always_deep_sleep();
  test_multicast_always_deep_sleep_helper();
  //test_multicast_polling();
  //test_multicast_different_helper();
}

void HelperMulticastBenchmark::test_multicast_different_helper() {
  MLOG_ERROR(mlog::app, "Start Multicast Helper test with different numbers of helper");
    mythos::PortalLock pl(portal);
    tc.init(manager.getNumThreads() - numHelper - 4);
    for (uint64_t i = 4; i < manager.getNumThreads() - numHelper; i++) {
      mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
      ASSERT(im.setPollingDelay(pl, ((uint32_t)-1)).wait());
      ASSERT(im.setLiteSleepDelay(pl, ((uint32_t)-1)).wait());
      manager.getThread(i)->signal();
    }
    pl.release();
    mythos::delay(1000000);
    while (not tc.isFinished()) {}


    MLOG_CSV(mlog::app,"GroupSize","Helper","Cycles");
    for (auto i = 5ul; i < manager.getNumThreads() - numHelper - 4; i += 5) {
      uint64_t min = ((uint64_t)-1);
      uint64_t min_h =((uint64_t)-1);
      for (auto h = 2ul; h < numHelper; h++) {
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

void HelperMulticastBenchmark::test_multicast_always_deep_sleep_helper() {
    uint64_t num = numHelper; // else macro will result in undefined reference
    MLOG_ERROR(mlog::app, "Start Multicast Helper test with", num, "helpers and always deep sleep");
    mythos::PortalLock pl(portal);
    tc.init(manager.getNumThreads() - numHelper - 4);
    for (uint64_t i = 4; i < manager.getNumThreads() - numHelper; i++) {
      mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
      ASSERT(im.setPollingDelay(pl, 0).wait());
      ASSERT(im.setLiteSleepDelay(pl, 0).wait());
      manager.getThread(i)->signal();
    }
    pl.release();
    mythos::delay(10000000);
    while (not tc.isFinished()) {}
    uint64_t values_helper[numHelper][46];
    for (uint64_t helper = 2; helper < numHelper; helper++) {
      MLOG_ERROR(mlog::app, "Testing with", helper);
      auto run = 0ul;
      for (uint64_t i = 2; i < 5; i++) {
          values_helper[helper][run] = test_multicast_ret(i, helper);
          run++;
      }
      for (uint64_t i = 5; i <= manager.getNumThreads() - numHelper - 4; i += 5) {
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
    for (uint64_t i = 5; i <= manager.getNumThreads() - numHelper - 4; i += 5) {
      MLOG_CSV(mlog::app,i,values_helper[2][run],values_helper[3][run],values_helper[4][run],values_helper[5][run],values_helper[6][run],values_helper[7][run],values_helper[8][run],values_helper[9][run],values_helper[10][run],values_helper[11][run],values_helper[12][run],values_helper[13][run],values_helper[14][run],values_helper[15][run],values_helper[16][run],values_helper[17][run],values_helper[18][run],values_helper[19][run],values_helper[20][run]);
        run++;
    }
    MLOG_ERROR(mlog::app, "End Multicast Helper test");
}

void HelperMulticastBenchmark::test_multicast_always_deep_sleep() {
    uint64_t num = numHelper; // else macro will result in undefined reference
    MLOG_ERROR(mlog::app, "Start Multicast Helper test with", num, "helpers and always deep sleep");
    mythos::PortalLock pl(portal);
    tc.init(manager.getNumThreads() - numHelper - 4);
    for (uint64_t i = 4; i < manager.getNumThreads() - numHelper; i++) {
      mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
      ASSERT(im.setPollingDelay(pl, 0).wait());
      ASSERT(im.setLiteSleepDelay(pl, 0).wait());
      manager.getThread(i)->signal();
    }
    pl.release();
    mythos::delay(10000000);
    while (not tc.isFinished()) {}

    for (uint64_t i = 2; i < 5; i++) {
        test_multicast_gen(i);
    }
    for (uint64_t i = 5; i <= manager.getNumThreads() - numHelper - 4; i += 5) {
        test_multicast_gen(i);
    }
    MLOG_ERROR(mlog::app, "End Multicast Helper test");
}

void HelperMulticastBenchmark::test_multicast_no_deep_sleep() {
    uint64_t num = numHelper; // else macro will result in undefined reference
    MLOG_ERROR(mlog::app, "Start Multicast Helper test with", num, "helpers and no deep sleep");

    mythos::PortalLock pl(portal);
    tc.init(manager.getNumThreads() - numHelper - 4);
    for (uint64_t i = 4; i < manager.getNumThreads() - numHelper; i++) {
      mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
      ASSERT(im.setPollingDelay(pl, 0).wait());
      ASSERT(im.setLiteSleepDelay(pl, (uint32_t(-1))).wait());
      manager.getThread(i)->signal();
    }
    pl.release();
    mythos::delay(10000000);
    while (not tc.isFinished()) {}

    for (uint64_t i = 2; i < 5; i++) {
        test_multicast_gen(i);
    }
    for (uint64_t i = 5; i <= manager.getNumThreads() - numHelper - 4; i += 5) {
        test_multicast_gen(i);
    }
    MLOG_ERROR(mlog::app, "End Multicast Helper test");
}

void HelperMulticastBenchmark::test_multicast_polling() {
    uint64_t num = numHelper; // else macro will result in undefined reference
    MLOG_ERROR(mlog::app, "Start Multicast Helper test with", num, "helpers and polling");

    mythos::PortalLock pl(portal);
    tc.init(manager.getNumThreads() - 4 - numHelper);
    for (uint64_t i = 4; i < manager.getNumThreads() - numHelper; i++) {
      mythos::IdleManagement im(mythos::init::IDLE_MANAGEMENT_START + i);
      ASSERT(im.setPollingDelay(pl, (uint32_t(-1))).wait());
      ASSERT(im.setLiteSleepDelay(pl, (uint32_t(-1))).wait());
      manager.getThread(i)->signal();
    }
    pl.release();
    while (not tc.isFinished()) {}
    mythos::delay(10000000);

    for (uint64_t i = 2; i < 5; i++) {
        test_multicast_gen(i);
    }
    for (uint64_t i = 5; i <= manager.getNumThreads() - numHelper - 4; i += 5) {
        test_multicast_gen(i);
    }
    MLOG_ERROR(mlog::app, "End Multicast Helper test");
}

void HelperMulticastBenchmark::test_multicast_gen(uint64_t numThreads, uint64_t helper_) {
    if (numThreads + 4 > manager.getNumThreads()- numHelper) {
      MLOG_ERROR(mlog::app, "Cannot test with", DVAR(numThreads));
      return;
    }

    mythos::PortalLock pl(portal);
    mythos::SignalGroup group(caps());
    ASSERT(group.create(pl, kmem, numThreads).wait());
    ASSERT(group.setCastStrategy(pl, HELPER));
    for (int i = 4; i < numThreads + 4; i++) {
        group.addMember(pl, manager.getThread(i)->ec).wait();
    }
    // register helper threads
    for (uint64_t i = 0; i < helper_; i++) {
        auto helper = manager.getNumThreads() - i - 1;
        group.addHelper(pl,mythos::init::HWTHREAD_START + helper).wait();
    }
    uint64_t sum = 0;
    mythos::Timer t;
    for (int j = 0; j < REPETITIONS; j++) {
      tc.init(numThreads);
      t.start();
      group.signalAll(pl);
      while(not tc.isFinished()) { /*mythos::hwthread_pause();*/ }
      sum += t.end();
      mythos::delay(100000); // wait to let threads enter deep sleep
    }

    MLOG_CSV(mlog::app, numThreads, sum/REPETITIONS);
    ASSERT(caps.free(group, pl));
}

uint64_t HelperMulticastBenchmark::test_multicast_ret(uint64_t numThreads, uint64_t helper_) {
    if (numThreads + 4 > manager.getNumThreads()- numHelper) {
      MLOG_ERROR(mlog::app, "Cannot test with", DVAR(numThreads));
      return 0;
    }
    mythos::PortalLock pl(portal);
    mythos::SignalGroup group(caps());
    ASSERT(group.create(pl, kmem, numThreads).wait());
    ASSERT(group.setCastStrategy(pl, HELPER));
    for (int i = 4; i < numThreads + 4; i++) {
        group.addMember(pl, manager.getThread(i)->ec).wait();
    }
    // register helper threads
    for (uint64_t i = 0; i < helper_; i++) {
        auto helper = manager.getNumThreads() - i - 1;
        group.addHelper(pl,mythos::init::HWTHREAD_START + helper).wait();
    }
    uint64_t min = ((uint64_t)-1);
    uint64_t sum = 0;
    mythos::Timer t;
    for (int j = 0; j < REPETITIONS; j++) {
      tc.init(numThreads);
      t.start();
      group.signalAll(pl);
      while(not tc.isFinished()) { /*mythos::hwthread_pause();*/ }
      auto value = t.end();
      sum += value;
      if (value < min) min = value;
      mythos::delay(100000); // wait to let threads enter deep sleep
    }

    //ASSERT(caps.free(group, pl));
    return sum/REPETITIONS;
    //return min;
}
