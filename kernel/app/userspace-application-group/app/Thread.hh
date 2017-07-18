#pragma once

#include "mythos/caps.hh"
#include "app/mlog.hh"

static constexpr size_t NUM_THREADS = 4;
static constexpr size_t PAGE_SIZE = 2 * 1024 * 1024;
static constexpr size_t STACK_SIZE = 1 * PAGE_SIZE;


class IThread {
public:
	virtual void* run(void *data);
};

enum ThreadState {
	READY   = 0,
	RUN,
	SLEEP,
	STOP,
	ZOMBIE,
};

struct Thread {
	mythos::CapPtr ec;
	mythos::CapPtr sc;
	void *stack_begin;
	void *stack_end;

	uint64_t id;
	std::atomic<ThreadState> state;
};

void* run(void *data)  {
	auto *thread = reinterpret_cast<Thread*>(data);
	MLOG_ERROR(mlog::app, DVAR(thread->id));

	while (true) {
		mythos::syscall_wait();
		MLOG_ERROR(mlog::app, "Resumed", DVAR(thread->id));
	}
}

class ThreadManager {
public:
	ThreadManager(mythos::Portal &portal_, mythos::CapMap cs_, mythos::PageMap as_, mythos::KernelMemory kmem_, mythos::SimpleCapAllocDel &caps_)
		: portal(portal_), cs(cs_), as(as_), kmem(kmem_), caps(caps_)
	{}
	void init();

	void startThread(Thread &t);

	void startAll();

	void deleteThread(Thread &t);

	Thread* getThread(uint64_t id) {
		if (id < NUM_THREADS) {
			return &threads[id];
		}
		return nullptr;
	}

	void wakeup(Thread &t);
private:
	void initMem();
	void initThreads();

private:
	Thread threads[NUM_THREADS];
	mythos::Portal &portal;
	mythos::CapMap cs;
	mythos::PageMap as;
	mythos::KernelMemory kmem;
	mythos::SimpleCapAllocDel &caps;
};

void ThreadManager::init() {
	initMem();
	initThreads();
}

void ThreadManager::initMem() {
	mythos::PortalLock pl(portal);
	mythos::Frame stackFrame(caps.alloc());
	auto res1 = stackFrame.create(pl, kmem, NUM_THREADS * STACK_SIZE, PAGE_SIZE).wait();
	ASSERT(res1);
	MLOG_INFO(mlog::app, "alloc thread stack frame", DVAR(res1.state()));
	constexpr uintptr_t VADDR = 12 * PAGE_SIZE;
	auto res2 = as.mmap(pl, stackFrame, VADDR, NUM_THREADS * STACK_SIZE,
	                    mythos::protocol::PageMap::MapFlags().writable(true)).wait();
	ASSERT(res2);
	MLOG_INFO(mlog::app, "mmap stack frame", DVAR(res2.state()),
	          DVARhex(res2->vaddr), DVARhex(res2->size), DVAR(res2->level));

	auto addr = reinterpret_cast<uint8_t*>(VADDR);
	for (auto& t : threads) {
		t.stack_end = addr;
		addr += STACK_SIZE;
		t.stack_begin = addr;
	}
}

void ThreadManager::initThreads() {
	auto id = 0;
	for (auto& t : threads) {
		t.id = id++;
		t.sc = mythos::init::SCHEDULERS_START + t.id;
		t.ec = caps.alloc();
		t.state.store(STOP);
	}
}

void ThreadManager::startThread(Thread &t) {
	mythos::PortalLock pl(portal);
	mythos::ExecutionContext thread(t.ec);
	t.state.store(RUN);
	auto res = thread.create(pl, kmem,
	                         as,
	                         cs,
	                         t.sc,
	                         t.stack_begin,
	                         &run,
	                         &t).wait();
	ASSERT(res);
}

void ThreadManager::startAll() {
	for (auto &t : threads) {
		startThread(t);
	}
}

void ThreadManager::deleteThread(Thread &t) {
	mythos::PortalLock pl(portal);
	mythos::CapMap capmap(cs);
	t.state.store(ZOMBIE);
	auto res = capmap.deleteCap(pl, t.ec).wait();
	ASSERT(res);
}

void ThreadManager::wakeup(Thread &t) {
	mythos::syscall_signal(t.ec);
}