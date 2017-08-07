#pragma once
#include "app/ThreadManager.hh"
#include "app/Thread.hh"
#include "runtime/SignalableGroup.hh"
#include "runtime/SimpleCapAlloc.hh"


extern ThreadManager manager;
extern mythos::SimpleCapAllocDel caps;
extern mythos::KernelMemory kmem;

class TreeMulticastBenchmark {
public:
	TreeMulticastBenchmark(mythos::PortalLock pl_)
		:pl(pl_) {}
public:
	void test_multicast1();

private:
	mythos::PortalLock pl;
};

void TreeMulticastBenchmark::test_multicast1() {
	// manager.init([](void *data) -> void* {
	// 	ASSERT(data != nullptr);
	// 	auto *thread = reinterpret_cast<Thread*>(data);
	// });
	// manager.startAll();
	Thread t;
	t.wait();

	mythos::SignalableGroup group(caps());
	TEST(group.create(pl, kmem, 100).wait());

}