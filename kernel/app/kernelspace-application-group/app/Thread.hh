#pragma once

#include "mythos/caps.hh"
#include <atomic>
#include "app/mlog.hh"

enum ThreadState {
	RUN = 0,
	SLEEP,
	STOP,
	ZOMBIE, // deleted
	INIT, //initialized
};

struct alignas(64) Thread {
	mythos::CapPtr ec;
	mythos::CapPtr sc;
	void *stack_begin;
	void *stack_end;

	uint64_t id; //linear thread id, thread runs on SC + id
	std::atomic<ThreadState> state {ZOMBIE};

	void*(*fun)(void*) = {nullptr};

	static void* run(void *data);
	static void  wait(Thread &t);
	static void  signal(Thread &t);

	void* run();
	void wait();
	void signal();
};