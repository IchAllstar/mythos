#pragma once

#include "mythos/caps.hh"
#include "app/ISignalable.hh"
#include "app/mlog.hh"
#include "app/Mutex.hh"

const size_t NUM_THREADS = 50;
const size_t NUM_HELPER_THREADS = 0;
const size_t PAGE_SIZE = 2 * 1024 * 1024;
const size_t STACK_SIZE = 1 * PAGE_SIZE;

SpinMutex global;

enum ThreadState {
	RUN = 0,
	SLEEP,
	STOP,
	ZOMBIE,
	INIT, //initialized
};

struct Thread : public ISignalable {
	mythos::CapPtr ec;
	mythos::CapPtr sc;
	void *stack_begin;
	void *stack_end;

	uint64_t id; //linear thread id, thread runs on SC + id
	std::atomic<ThreadState> state {ZOMBIE};
	std::atomic<bool> SIGNALLED = {false};
	SpinMutex threadMtx;

	void*(*fun)(void*) = {nullptr};

	static void* run(void *data);
	static void  wait(Thread &t);
	static void  signal(Thread &t);
public: // ISignalable Interface
	void signal() override {
		signal(*this);
	}

  void forwardMulticast() override {
    auto &mc = this->cast;
    //Multicast mc;
    //this->cast.read(mc);
    ASSERT(mc.group != nullptr);
    ASSERT(mc.idx == this->id -1);
    for (size_t i = 0; i < mc.N; ++i) { // for all children in tree
			size_t child_idx = mc.idx * mc.N + i + 1;
			if (child_idx >= mc.groupSize) {
				return;
			}
      //MLOG_ERROR(mlog::app, DVAR(this->id), DVAR(mc.idx), DVAR(child_idx));
			ISignalable *signalable = mc.group[child_idx]; // child
			if (signalable) {
        // setup and signal
				signalable->cast.set(mc.group, mc.groupSize, child_idx, mc.N);
        signalable->signal();
			} else {
				MLOG_ERROR(mlog::app, "Child not there", DVAR(child_idx));
			}
		}
  }
};

void* Thread::run(void *data) {
	auto *thread = reinterpret_cast<Thread*>(data);
  ASSERT(thread != nullptr);
	//MLOG_INFO(mlog::app, "Started Thread", thread->id);
	while (true) {
    auto prev = thread->SIGNALLED.exchange(false);
    if (prev) {
      if (thread->cast.onGoing.load() == true) {
        thread->forwardMulticast();
        thread->cast.reset();
      }
    }

    if (thread->fun != nullptr) {
			thread->fun(data);
		}

		wait(*thread);
	}
}

void Thread::wait(Thread &t) {
	if (t.SIGNALLED.load()) {
		return;
	}
	//t.state.store(STOP);
	t.state.store(STOP);
	mythos::syscall_wait();
}

void Thread::signal(Thread &t) {
	//MLOG_ERROR(mlog::app, "send signal to Thread", t.id);
  t.state.store(RUN);
	auto prev = t.SIGNALLED.exchange(true);
	if (not prev) {
    //LockGuard<SpinMutex> g(global);
	  mythos::syscall_signal(t.ec);
	}
}

class ThreadManager {
public:
	ThreadManager(mythos::Portal &portal_, mythos::CapMap cs_, mythos::PageMap as_, mythos::KernelMemory kmem_, mythos::SimpleCapAllocDel &caps_)
		: portal(portal_), cs(cs_), as(as_), kmem(kmem_), caps(caps_)
	{}
	void init(void*(*fun)(void*) = nullptr);
	void startThread(Thread &t);
	void startAll();
	void deleteThread(Thread &t);
  void registerHelper(Thread &t);
  bool isHelper(Thread &t);
  Thread* getHelper(uint64_t number);
	uint64_t getNumThreads() {
		return NUM_THREADS;
	}

	Thread* getThread(uint64_t id) {
		if (id < NUM_THREADS) {
			return &threads[id];
		}
		return nullptr;
	}

private:
	void initMem();
	void initThreads(void*(*fun_)(void*));

private:
	Thread threads[NUM_THREADS];
  Thread* helperThreads[NUM_HELPER_THREADS];
	mythos::Portal &portal;
	mythos::CapMap cs;
	mythos::PageMap as;
	mythos::KernelMemory kmem;
	mythos::SimpleCapAllocDel &caps;
};

void ThreadManager::init(void*(*fun_)(void*)) {
	initMem();
	initThreads(fun_);
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

void ThreadManager::initThreads(void*(*fun_)(void*)) {
	auto id = 0;
	for (auto& t : threads) {
    t.id = id++;
		t.sc = mythos::init::SCHEDULERS_START + t.id;
		t.fun = fun_;
		t.ec = caps.alloc();
		t.state.store(INIT);
	}
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
  MLOG_ERROR(mlog::app, DVAR(t.id), DVAR(t.ec), DVAR(t.sc), DVAR(t.stack_begin));
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

void ThreadManager::registerHelper(Thread &t) {
  for (uint64_t i = 0; i < NUM_HELPER_THREADS; ++i) {
    if (helperThreads[i] == nullptr) {
        helperThreads[i] = &t;
        MLOG_ERROR(mlog::app, "register helper at", i);
    }
  }
  PANIC("No free helper space");
}

Thread* ThreadManager::getHelper(uint64_t number) {
  ASSERT(number < NUM_HELPER_THREADS);
  return helperThreads[number];
}

bool ThreadManager::isHelper(Thread &t) {
  for (uint64_t i = 0; i < NUM_HELPER_THREADS; ++i) {
    if (helperThreads[i] == &t) {
      MLOG_ERROR(mlog::app, "is helper", i);
      return true;
    }
  }
  return false;
}
