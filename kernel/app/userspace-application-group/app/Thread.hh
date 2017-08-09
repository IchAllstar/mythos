#pragma once

#include "mythos/caps.hh"
#include "app/ISignalable.hh"
#include "app/mlog.hh"
#include "app/Mutex.hh"
#include "mythos/syscall.hh"


//SpinMutex global;

enum ThreadState {
	RUN = 0,
	SLEEP,
	STOP,
	ZOMBIE, // deleted
	INIT, //initialized
};

struct alignas(64) Thread : public ISignalable {
public:
	mythos::CapPtr ec;
	mythos::CapPtr sc;
	void *stack_begin;
	void *stack_end;
	uint64_t id; //linear thread id, thread runs on SC + id
	std::atomic<ThreadState> state {ZOMBIE};

	std::atomic<bool> SIGNALLED = {false};

	void*(*fun)(void*) = {nullptr};
public:
  Task::list_t taskQueue;
public:
	static void* run(void *data);
	static void  wait(Thread &t);
	static void  signal(Thread &t);

public: // ISignalable Interface
	void signal() override {
		//MLOG_ERROR(mlog::app, "In Signal interface funct");
		signal(*this);
	}

	void addTask(Task::list_t::Queueable *q) override {
		taskQueue.push(q);
	}
};
