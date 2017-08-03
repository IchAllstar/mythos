#pragma once
#include "conf.hh"
#include "runtime/Portal.hh"
#include "runtime/ExecutionContext.hh"
#include "runtime/CapMap.hh"
#include "runtime/Example.hh"
#include "runtime/InterruptControl.hh"
#include "runtime/PageMap.hh"
#include "runtime/KernelMemory.hh"
#include "runtime/SimpleCapAlloc.hh"
#include "app/Thread.hh"

class Thread;

class ThreadManager {
public:
	ThreadManager(mythos::Portal &portal_, mythos::CapMap cs_, mythos::PageMap as_, mythos::KernelMemory kmem_, mythos::SimpleCapAllocDel &caps_)
		: portal(portal_), cs(cs_), as(as_), kmem(kmem_), caps(caps_)
	{}
	void init(void*(*fun)(void*) = nullptr);
	void initThread(uint64_t id, void*(*fun)(void*) = nullptr);
	void startThread(Thread &t);
	void startAll();
	void deleteThread(Thread &t);
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