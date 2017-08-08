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
	void test_multicast();
	void test_multicast_gen(uint64_t);

private:
	mythos::Portal &portal;
};

void TreeMulticastBenchmark::test_multicast() {
	MLOG_ERROR(mlog::app, "Start Multicast tree test");
	for (uint64_t i = 5; i < 60; i += 5) {
		test_multicast_gen(i);
	}
	MLOG_ERROR(mlog::app, "ENd Multicast tree test");
}

void TreeMulticastBenchmark::test_multicast_gen(uint64_t numThreads) {
	counter.store(0);
	manager.init([](void *data) -> void* {
		counter.fetch_add(1);
	});
	manager.startThreadRange(1, numThreads + 1);

	mythos::PortalLock pl(portal);
	mythos::SignalableGroup group(caps());
	TEST(group.create(pl, kmem, numThreads).wait());
	for (int i = 1; i < numThreads + 1; i++) {
		group.addMember(pl, manager.getThread(i)->ec).wait();
	}
	while (counter.load() != numThreads) { mythos::hwthread_pause(); }
	counter.store(0);
	mythos::Timer t;
	t.start();
	group.signalAll(pl).wait();
	while (counter.load() != numThreads) { mythos::hwthread_pause(); }
	MLOG_ERROR(mlog::app, DVAR(numThreads), DVAR(t.end()));
	caps.free(group, pl);
	//manager.cleanup();
}