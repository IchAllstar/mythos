#include "app/ThreadManager.hh"
#include "runtime/HWThread.hh"

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
	LockGuard<SpinMutex> g(lock);
	mythos::PortalLock pl(portal);

  //allocate stacks
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


  // allocate portals
  uintptr_t PORTAL_ADDR = VADDR + NUM_THREADS * STACK_SIZE + PAGE_SIZE;
  IB_START = PORTAL_ADDR;
	mythos::Frame ibFrame(caps.alloc());
  ibFrameCap = ibFrame.cap();
	ASSERT(ibFrame.create(pl, kmem, NUM_THREADS * PAGE_SIZE, PAGE_SIZE).wait());
	MLOG_ERROR(mlog::app, "alloc thread ib frame");
	auto res3 = as.mmap(pl, ibFrame, PORTAL_ADDR, NUM_THREADS * PAGE_SIZE,
	                    mythos::protocol::PageMap::MapFlags().writable(true)).wait();
	ASSERT(res3);
	MLOG_ERROR(mlog::app, "mmap ib frame", DVAR(res3.state()),
	          DVARhex(res3->vaddr), DVARhex(res3->size), DVAR(res3->level));
	auto ib_addr = reinterpret_cast<uint8_t*>(PORTAL_ADDR);

  // set things
	for (auto& t : threads) {
		t.stack_end = addr;
    t.ib = ib_addr;
    ib_addr += PAGE_SIZE;
		addr += STACK_SIZE;
		t.stack_begin = addr;
	}
}

void ThreadManager::initThreads(void*(*fun_)(void*)) {
	initMem();
	auto id = 0;
	for (auto& t : threads) {
		t.id = id;
		t.sc = mythos::init::SCHEDULERS_START + t.id;
    t.hwt = mythos::init::HWTHREAD_START + t.id;
		t.fun = fun_;
		t.ec = caps.alloc();
    t.portal = caps.alloc();
		t.state.store(INIT);
    id++;
	}
}

void ThreadManager::initThread(uint64_t id, void*(*fun_)(void*)) {
	initMem();
	auto &t = threads[id];
	t.id = id;
	t.sc = mythos::init::SCHEDULERS_START + t.id;
  t.hwt = mythos::init::HWTHREAD_START + t.id;
	t.fun = fun_;
	t.ec = caps.alloc();
  t.portal = caps.alloc();
	t.state.store(INIT);
}

void ThreadManager::startThread(Thread &t) {
	LockGuard<SpinMutex> g(lock);
	mythos::PortalLock pl(portal);
	if (t.state.load() != ZOMBIE && t.state.load() != INIT) {
		MLOG_ERROR(mlog::app, "Thread already initialized, signalling instead", DVAR(t.id));
		Thread::signal(t);
		return;
	}

  mythos::Portal p(t.portal, t.ib);
  ASSERT(p.create(pl, kmem).wait());
	mythos::ExecutionContext thread(t.ec);
	t.state.store(RUN);
	auto res = thread.create(pl,
	                         kmem,
	                         mythos::init::PML4,
	                         mythos::init::CSPACE,
	                         t.sc,
	                         t.stack_begin,
	                         &Thread::run,
	                         &t).wait();
	ASSERT(res);
  ASSERT(p.bind(pl, mythos::Frame(ibFrameCap), (uintptr_t)t.ib - IB_START, t.ec).wait());
}

void ThreadManager::startAll() {
	for (auto &t : threads) {
		startThread(t);
	}
}

void ThreadManager::deleteThread(Thread &t) {
	LockGuard<SpinMutex> g(lock);
  mythos::PortalLock pl(portal);
	mythos::CapMap capmap(cs);
	t.state.store(ZOMBIE);
	auto res = capmap.deleteCap(pl, t.ec).wait();
	ASSERT(res);
}

void ThreadManager::cleanup() {
	for (uint64_t i = 0; i < NUM_THREADS; ++i) {
		auto *t = getThread(i);
		ASSERT(t);
		deleteThread(*t);
	}
}

uint64_t ThreadManager::getSleepState(uint64_t context_id, uint64_t dest_id) {
  ASSERT(context_id < NUM_THREADS && dest_id < NUM_THREADS);
  //MLOG_ERROR(mlog::app, DVAR(context_id), DVAR(dest_id));
  auto ctx = getThread(context_id);
  if (not ctx) return 0;
  mythos::Portal p(ctx->portal, ctx->ib);
  mythos::PortalLock pl(p);
  mythos::HWThread hwt(mythos::init::HWTHREAD_START + dest_id);
  auto res = hwt.readSleepState(pl).wait();
  if (not res) MLOG_ERROR(mlog::app, res.state());
  //MLOG_ERROR(mlog::app, DVAR(res->sleep_state));
  return res->sleep_state;
}
