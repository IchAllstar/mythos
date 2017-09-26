#pragma once

#include "mythos/caps.hh"
#include "app/ISignalable.hh"
#include "app/mlog.hh"
#include "app/Mutex.hh"
#include "mythos/syscall.hh"
#include "runtime/Portal.hh"

static SpinMutex global;

enum ThreadState {
	RUN = 0,
	SLEEP,
	STOP,
	ZOMBIE, // deleted
	INIT, //initialized
};

/**
 * Simple Thread abstraction which executes a provided function.
 * Runs in a loop: handles taskqueue->executes user function->wait()
 */
struct alignas(64) Thread : public ISignalable {
public:
	mythos::CapPtr ec;
	mythos::CapPtr sc;
  mythos::CapPtr hwt;
  mythos::CapPtr portal;

  void *ib;
	void *stack_begin;
	void *stack_end;
	uint64_t id; //linear thread id, thread runs on SC + id
	std::atomic<ThreadState> state {ZOMBIE};

	std::atomic<bool> SIGNALLED = {false};

	void*(*fun)(void*) = {nullptr};
public:
  // Task Queue which is handled at the beginning of a thread loop
  Task::list_t taskQueue;
public:
	static void* run(void *data);
	static void  wait(Thread &t);
	static void  signal(Thread &t);


public: // ISignalable Interface
	void signal() override {
    // Fixes the Deadlock situation
    //LockGuard<SpinMutex> g(global);
		signal(*this);
	}

  uint64_t getID() override;
  void addTask(Task::list_t::Queueable *q) override;
};
