#pragma once
#include "runtime/Portal.hh"
#include "runtime/ExecutionContext.hh"
#include "runtime/CapMap.hh"
#include "runtime/Example.hh"
#include "runtime/InterruptControl.hh"
#include "runtime/PageMap.hh"
#include "runtime/KernelMemory.hh"
#include "runtime/SimpleCapAlloc.hh"
#include "app/Thread.hh"

static const uint64_t NUM_THREADS = 60;
static const size_t PAGE_SIZE = 2 * 1024 * 1024;
static const size_t STACK_SIZE = 1 * PAGE_SIZE;

class Thread;

class ThreadManager {
public:
	ThreadManager(mythos::Portal &portal_, mythos::CapMap cs_, mythos::PageMap as_, mythos::KernelMemory kmem_, mythos::SimpleCapAllocDel &caps_)
		: portal(portal_), cs(cs_), as(as_), kmem(kmem_), caps(caps_)
	{}
	void init(void*(*fun)(void*) = nullptr);
	void initThread(uint64_t id, void*(*fun)(void*) = nullptr);
	void startThreadRange(uint64_t from, uint64_t to);
	void startThread(Thread &t);
	void startAll();
	void deleteThread(Thread &t);
	void cleanup();
	uint64_t getNumThreads() { return NUM_THREADS; }

	Thread* getThread(uint64_t id);
private:
	void initMem();
	void initThreads(void*(*fun_)(void*));

private:
	std::atomic<bool> memoryInitialized {false};
	Thread threads[NUM_THREADS];
	mythos::Portal &portal;
	mythos::CapMap cs;
	mythos::PageMap as;
	mythos::KernelMemory kmem;
	mythos::SimpleCapAllocDel &caps;
};