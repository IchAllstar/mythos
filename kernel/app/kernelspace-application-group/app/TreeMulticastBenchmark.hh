#pragma once
#include "app/ThreadManager.hh"
#include "app/Thread.hh"
#include "runtime/SignalableGroup.hh"
#include "runtime/SimpleCapAlloc.hh"
#include <atomic>
#include "util/Time.hh"


extern ThreadManager manager;
extern mythos::SimpleCapAllocDel caps;
extern mythos::KernelMemory kmem;
extern std::atomic<uint64_t> counter;

class TreeMulticastBenchmark {
public:
	TreeMulticastBenchmark(mythos::Portal &pl_)
		: portal(pl_) {}
public:
	void setup();
	void test_multicast();
	void test_multicast_gen(uint64_t);

private:
	mythos::Portal &portal;
};

void TreeMulticastBenchmark::setup() {
	manager.init([](void *data) -> void* {
		counter.fetch_add(1);
		//MLOG_ERROR(mlog::app, "Here");
	});
	manager.startAll();
	while (counter.load() != manager.getNumThreads() - 1) {}
	counter.store(0);
}

void TreeMulticastBenchmark::test_multicast() {
	MLOG_ERROR(mlog::app, "Start Multicast tree test");
	setup();
	for (uint64_t i = 5; i < manager.getNumThreads(); i += 5) {
		test_multicast_gen(i);
	}
	MLOG_ERROR(mlog::app, "ENd Multicast tree test");
}

extern uint64_t repetitions;
void TreeMulticastBenchmark::test_multicast_gen(uint64_t numThreads) {
	mythos::PortalLock pl(portal);
	mythos::SignalableGroup group(caps());
  mythos::SignalableGroup group2(caps());
	ASSERT(group.create(pl, kmem, numThreads).wait());
	ASSERT(group2.create(pl, kmem, numThreads).wait());
	for (int i = 1; i < numThreads + 1; i++) {
		group.addMember(pl, manager.getThread(i)->ec).wait();
		group2.addMember(pl, manager.getThread(i)->ec).wait();
	}
	mythos::Timer t;
	uint64_t sum = 0;
	for (uint64_t i = 0; i < repetitions; i++) {
		counter.store(0);
		t.start();
		group.signalAll(pl).wait();
    group2.signalAll(pl).wait();
		while (counter.load() != numThreads) { /*mythos::hwthread_pause();*/ }
		sum += t.end();
		//mythos::hwthread_pause(1);
	}
	MLOG_ERROR(mlog::app, DVAR(numThreads), DVAR(sum / repetitions));
	caps.free(group, pl);
	//manager.cleanup();
}
