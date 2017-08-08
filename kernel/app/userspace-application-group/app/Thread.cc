#include "app/Thread.hh"

SpinMutex global;

void Thread::forwardMulticast() {
	auto &mc = this->cast;
	//Multicast mc;
	//this->cast.read(mc);
	ASSERT(mc.group != nullptr);
	ASSERT(mc.idx == this->id - 1);
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

void* Thread::run(void *data) {
	auto *thread = reinterpret_cast<Thread*>(data);
	ASSERT(thread != nullptr);
	//MLOG_INFO(mlog::app, "Started Thread", thread->id);
	while (true) {
		//MLOG_ERROR(mlog::app, "Thread loop", DVAR(thread->id));
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

  //t.state.store(RUN);
	auto prev = t.SIGNALLED.exchange(true);
	if (not prev) {
		//LockGuard<SpinMutex> g(global);
		mythos::syscall_signal(t.ec);
	}
}

