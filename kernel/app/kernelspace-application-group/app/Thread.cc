#include "app/Thread.hh"
#include "mythos/syscall.hh"

void* Thread::run(void *data) {
	auto *thread = reinterpret_cast<Thread*>(data);
	ASSERT(thread != nullptr);
	thread->run();
}


void Thread::wait(Thread &t) {
	mythos::syscall_wait();
}

void Thread::signal(Thread &t) {
	mythos::syscall_signal(t.ec);
}

void* Thread::run() {
	while (true) {
		if (fun != nullptr) {
			fun(this);
		}
		wait();
	}
}

void Thread::wait() {
	Thread::wait(*this);
}

void Thread::signal() {
	Thread::signal(*this);
}
