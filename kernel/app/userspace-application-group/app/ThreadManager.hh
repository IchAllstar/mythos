#pragma once

const size_t NUM_THREADS = 64;
const size_t NUM_HELPER =  10;
const size_t PAGE_SIZE = 2 * 1024 * 1024;
const size_t STACK_SIZE = 1 * PAGE_SIZE;

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
  void registerHelper(Thread &t);
  bool isHelper(Thread &t);
  Thread* getHelper(uint64_t number);
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

Thread* ThreadManager::getThread(uint64_t id) {
		if (id < NUM_THREADS) {
			return &threads[id];
		}
		return nullptr;
}

void ThreadManager::init(void*(*fun_)(void*)) {
	initThreads(fun_);
}

void ThreadManager::initMem() {
  auto prev = memoryInitialized.exchange(true);
  if (prev) {
    return;
  }
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

void ThreadManager::initThreads(void*(*fun_)(void*)) {
  initMem();
	auto id = 0;
	for (auto& t : threads) {
    t.id = id++;
		t.sc = mythos::init::SCHEDULERS_START + t.id;
		t.fun = fun_;
		t.ec = caps.alloc();
		t.state.store(INIT);
	}
}

void ThreadManager::initThread(uint64_t id, void*(*fun_)(void*)) {
    initMem();
    auto &t = threads[id];
    t.id = id;
		t.sc = mythos::init::SCHEDULERS_START + t.id;
		t.fun = fun_;
		t.ec = caps.alloc();
		t.state.store(INIT);
}

void ThreadManager::startThread(Thread &t) {
	mythos::PortalLock pl(portal);
	if (t.state.load() != ZOMBIE && t.state.load() != INIT) {
		MLOG_ERROR(mlog::app, "Thread already initialized", DVAR(t.id));
    Thread::signal(t);
		return;
	}
	mythos::ExecutionContext thread(t.ec);
	t.state.store(RUN);
  //MLOG_ERROR(mlog::app, DVAR(t.id), DVAR(t.ec), DVAR(t.sc), DVAR(t.stack_begin));
	auto res = thread.create(pl,
                           kmem,
	                         mythos::init::PML4,
	                         mythos::init::CSPACE,
	                         t.sc,
	                         t.stack_begin,
	                         &Thread::run,
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

