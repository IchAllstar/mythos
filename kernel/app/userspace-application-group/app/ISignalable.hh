#pragma once

#include "app/Thread.hh"
#include "app/Mutex.hh"
#include "app/Task.hh"
#include <array>

class ISignalable;
class Thread;

class SignalableGroup;

struct Multicast {
	std::atomic<bool> onGoing {false};
	ISignalable **group;
	size_t groupSize {0};
	size_t idx {0}; // index in array for calculation of children
	size_t N {0}; //N-ary Tree
  SpinMutex mutex;

	void set(ISignalable **group_, size_t groupSize_, size_t idx_, size_t N_) {
    mutex.lock();
    while (onGoing.exchange(true) == true) {
      mythos::hwthread_pause(100);
      //MLOG_ERROR(mlog::app, "Waiting for ongoing cast to end");
		}
		// possible race here with setting and reading the values
		group = group_;
		groupSize = groupSize_;
		idx = idx_;
		N = N_;
    mutex.unlock();
	}

  void read(Multicast &mc) {
    mutex.lock();
    mc.group = group;
    mc.groupSize = groupSize;
    mc.idx = idx;
    mc.N = N;
    mutex.unlock();
  }

	void reset() {
    //MLOG_ERROR(mlog::app, "reset", DVAR(idx));
		ASSERT(onGoing.exchange(false) == true);
    group = nullptr;
    idx = 0;
    N = 0;
	}
};

class ISignalable {
public:
	virtual void signal() = 0;
  virtual void addTask(Task::list_t::Queueable *q) = 0;
  virtual ~ISignalable() {}
};
