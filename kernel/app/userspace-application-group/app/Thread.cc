#include "app/Thread.hh"
#include "app/ThreadManager.hh"

extern ThreadManager manager;

void* Thread::run(void *data) {
	auto *thread = reinterpret_cast<Thread*>(data);
	ASSERT(thread != nullptr);
	MLOG_DETAIL(mlog::app, "Started Thread", thread->id);
	while (true) {
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
	t.state.store(STOP);
	mythos::syscall_wait();
}

void Thread::signal(Thread &t) {
  t.state.store(RUN);
	auto prev = t.SIGNALLED.exchange(true);
	if (not prev) {
		mythos::syscall_signal(t.ec);
	}
}

uint64_t Thread::getID() {
  return this->id;
}

void Thread::addTask(Task::list_t::Queueable *q) {
  taskQueue.push(q);
}
