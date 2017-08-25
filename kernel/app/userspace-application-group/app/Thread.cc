#include "app/Thread.hh"

//SpinMutex global;

void* Thread::run(void *data) {
	auto *thread = reinterpret_cast<Thread*>(data);
	ASSERT(thread != nullptr);
	MLOG_DETAIL(mlog::app, "Started Thread", thread->id);
	while (true) {
		//MLOG_ERROR(mlog::app, "Thread loop", DVAR(thread->id));
		while (!thread->taskQueue.empty()) {
			auto *t = thread->taskQueue.pull();
			t->get()->run();
		}
		if (thread->SIGNALLED.exchange(false)) {
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
	mythos::syscall_wait();
}

void Thread::signal(Thread &t) {
	//MLOG_ERROR(mlog::app, "send signal to Thread", t.id);
  //mythos::syscall_signal(t.ec);

  //t.state.store(RUN);
	auto prev = t.SIGNALLED.exchange(true);
	if (not prev) {
		//LockGuard<SpinMutex> g(global);
		mythos::syscall_signal(t.ec);
	}

}

